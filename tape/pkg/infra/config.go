package infra

import (
	"io/ioutil"

	"github.com/gogo/protobuf/proto"
	"github.com/hyperledger/fabric-protos-go/msp"
	"github.com/pkg/errors"
	"gopkg.in/yaml.v2"
)

type Config struct {
	Endorsers       []Node   `yaml:"endorsers"`
	Committer       Node     `yaml:"committer"`
	Orderer         Node     `yaml:"orderer"`
	Channel         string   `yaml:"channel"`
	Chaincode       string   `yaml:"chaincode"`
	Version         string   `yaml:"version"`
	Args            []string `yaml:"args"`
	MSPID           string   `yaml:"mspid"`
	PrivateKey      string   `yaml:"private_key"`
	SignCert        string   `yaml:"sign_cert"`
	NumOfConn       int      `yaml:"num_of_conn"`
	ClientPerConn   int      `yaml:"client_per_conn"`
	OrdererClient   int      `yaml:"orderer_client"`
	EndorserGroups  int      `yaml:"endorser_groups"`
	Check_Txid      bool     `yaml:"check_txid"`
	Check_rwset     bool     `yaml:"check_rwset"`
	End2end         bool     `yaml:"e2e"`
	Hot_rate        float64  `yaml:"hot_rate"`
	Contention_rate float64  `yaml:"contention_rate"`
	Threads         int      `yaml:"threads"`
}

type Node struct {
	Addr          string `yaml:"addr"`
	TLSCACert     string `yaml:"tls_ca_cert"`
	TLSCAKey      string `yaml:"tls_ca_key"`
	TLSCARoot     string `yaml:"tls_ca_root"`
	TLSCACertByte []byte
	TLSCAKeyByte  []byte
	TLSCARootByte []byte
}

func LoadConfig(f string) (Config, error) {
	config := Config{}
	raw, err := ioutil.ReadFile(f)
	if err != nil {
		return config, errors.Wrapf(err, "error loading %s", f)
	}
	err = yaml.Unmarshal(raw, &config)
	if err != nil {
		return config, errors.Wrapf(err, "error unmarshal %s", f)
	}

	for i, _ := range config.Endorsers {
		err = config.Endorsers[i].loadConfig()
		if err != nil {
			return config, err
		}
	}
	err = config.Committer.loadConfig()
	if err != nil {
		return config, err
	}
	err = config.Orderer.loadConfig()
	if err != nil {
		return config, err
	}
	return config, nil
}

func (c Config) LoadCrypto() (*Crypto, error) {
	var allcerts []string
	for _, p := range c.Endorsers {
		allcerts = append(allcerts, p.TLSCACert)
	}
	allcerts = append(allcerts, c.Orderer.TLSCACert)

	conf := CryptoConfig{
		MSPID:    c.MSPID,
		PrivKey:  c.PrivateKey,
		SignCert: c.SignCert,
	}

	priv, err := GetPrivateKey(conf.PrivKey)
	if err != nil {
		return nil, errors.Wrapf(err, "error loading priv key")
	}

	cert, certBytes, err := GetCertificate(conf.SignCert)
	if err != nil {
		return nil, errors.Wrapf(err, "error loading certificate")
	}

	id := &msp.SerializedIdentity{
		Mspid:   conf.MSPID,
		IdBytes: certBytes,
	}

	name, err := proto.Marshal(id)
	if err != nil {
		return nil, errors.Wrapf(err, "error get msp id")
	}

	return &Crypto{
		Creator:  name,
		PrivKey:  priv,
		SignCert: cert,
	}, nil
}

func GetTLSCACerts(file string) ([]byte, error) {
	if len(file) == 0 {
		return nil, nil
	}

	in, err := ioutil.ReadFile(file)
	if err != nil {
		return nil, errors.Wrapf(err, "error loading %s", file)
	}
	return in, nil
}

func (n *Node) loadConfig() error {
	TLSCACert, err := GetTLSCACerts(n.TLSCACert)
	if err != nil {
		return errors.Wrapf(err, "fail to load TLS CA Cert %s", n.TLSCACert)
	}
	certPEM, err := GetTLSCACerts(n.TLSCAKey)
	if err != nil {
		return errors.Wrapf(err, "fail to load TLS CA Key %s", n.TLSCAKey)

	}
	TLSCARoot, err := GetTLSCACerts(n.TLSCARoot)
	if err != nil {
		return errors.Wrapf(err, "fail to load TLS CA Root %s", n.TLSCARoot)
	}
	n.TLSCACertByte = TLSCACert
	n.TLSCAKeyByte = certPEM
	n.TLSCARootByte = TLSCARoot
	return nil
}
