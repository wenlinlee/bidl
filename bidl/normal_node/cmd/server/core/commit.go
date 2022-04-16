package core

import (
	"strconv"
	"normal_node/cmd/server/util"
        "time"
	log "github.com/sirupsen/logrus"
         
)

func (p *Processor) commitTxn(hashes [][32]byte) {
	log.Debugf("Commiting transaction state.")
	var elapsedBlk int64 = 0	
	var commitTxn  int64 = 0
        var relatetx int64 = 0
	for i := 0; i < len(hashes); i++ {
		hash := hashes[i]
		if _, ok := p.Envelops[hash]; ok {
			env := p.Envelops[hash]
			wset := env.WSet
                        if len(wset) == 1 {
                           relatetx++ }
                        if !env.ND {
				log.Debugf("The transaction is deterministic")
				if env.SeqTransaction.HasPersisted == true && (len(wset) != 0) {
					sequenced_time := env.SeqTransaction.Transaction.Timestamp 
                                        commited_time := time.Now().UnixNano()
					commitTxn++
					end_to_end_time := commited_time - sequenced_time
					elapsedBlk += end_to_end_time
					for k, v := range wset {
						p.DB.Put([]byte(strconv.Itoa(k)), []byte(strconv.Itoa(v)), nil)
					}
					//log.Infof("The transaction commited, it seqNum is %d, The end_to_end_time is %d ms. ", env.SeqTransaction.Seq, end_to_end_time/2e6)
					delete(p.Envelops, hash)
                                        util.Monitor.TputTxn <- 1
				} else {
					p.Envelops[hash].SeqTransaction.HasCommited = true
					//log.Infof("The transaction commited but it is non-Persisted, it seqNum is %d!", env.SeqTransaction.Seq)
				}
			} else {
				log.Debugf("The transaction is non-deterministic")
			}
		} else {
		log.Infof("I dont have this hash !")
		}
	}
	if commitTxn == 0 {
		log.Infof("commit Transaction end, The block tx Number is %d, commited tx number is %d. ", len(hashes), commitTxn)
	} else {
		log.Infof("commit Transaction end, relate tx %d, commited tx number is %d, end_to_end_time is %.3f ms, total time %d ms", relatetx, commitTxn, float32((elapsedBlk/commitTxn)/1e6), elapsedBlk/1e6)
	}
}

func (p *Processor) commitBlock(block []byte) {
	log.Debugf("Commiting block.")
	p.DB.Put([]byte("block"+strconv.Itoa(p.blkNum)), block, nil)
}
