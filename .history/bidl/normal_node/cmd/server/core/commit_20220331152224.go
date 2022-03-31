package core

import (
	"strconv"

	"normal_node/cmd/server/util"

	log "github.com/sirupsen/logrus"
)

func (p *Processor) commitTxn(hashes [][32]byte) {
	log.Debugf("Commiting transaction state.")
	var elapsedBlk int64 = 0	
	var commitTxn  int64 = 0
	for i := 0; i < len(hashes); i++ {
		hash := hashes[i]
		if _, ok := p.Envelops[hash]; ok {
			env := p.Envelops[hash]
			wset := env.WSet
			if !env.ND {
				log.Debugf("The transaction is deterministic")
				if env.SeqTransaction.HasPersisted {
					sequenced_time := env.SeqTransaction.Transaction.Timestamp 
			        commited_time := time.Now().UnixNano()
					commitTxn++
					end_to_end_time := commited_time - sequenced_time
					elapsedBlk += end_to_end_time
					for k, v := range wset {
						p.DB.Put([]byte(strconv.Itoa(k)), []byte(strconv.Itoa(v)), nil)
					}
					//hashstr := (*string)(unsafe.Pointer(&hash))
					log.Infof("The transaction commited, delete Envelop data! it seqNum is %d, hascommited value is %s, haspersisted value is %s! The end_to_end_time is %d ms, %d transaction total time is %d ", env.SeqTransaction.Seq, strconv.FormatBool(env.SeqTransaction.HasCommited), strconv.FormatBool(env.SeqTransaction.HasPersisted), end_to_end_time/1e6,  commitTxn, elapsedBlk/1e6)
					util.Monitor.TputTxn <- 1
					delete(p.Envelops, hash)
				} else {
					env.SeqTransaction.HasCommited = true
					//hashstr := (*string)(unsafe.Pointer(&hash))
					//log.Infof("The transaction commited but it is non-Persisted, it seqNum is %d, hascommited value is %s, haspersisted value is %s!", env.SeqTransaction.Seq, strconv.FormatBool(env.SeqTransaction.HasCommited), strconv.FormatBool(env.SeqTransaction.HasPersisted))
				}
			} else {
				log.Debugf("The transaction is non-deterministic")
			}
		} else {
			log.Debugf("I don't hold the hash")
		}
	}
	if commitTxn == 0 {
		log.Infof("commit Transaction end, The block tx Number is %d, commited tx number is %d. ", len(hashes), commitTxn)
	} else {
		log.Infof("commit Transaction end, The block tx Number is %d, commited tx number is %d, the average end_to_end_time is %.3f ms", len(hashes), commitTxn, float32((elapsedBlk/commitTxn)/1e6))
	}
}

func (p *Processor) commitBlock(block []byte) {
	log.Debugf("Commiting block.")
	p.DB.Put([]byte("block"+strconv.Itoa(p.blkNum)), block, nil)
}
