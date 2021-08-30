#!/bin/bash -e

script_dir=$(cd "$(dirname "$0")";pwd)
source $script_dir/env.sh

echo "Stopping sequencer/consensus/normal nodes..."
source $script_dir/kill_all_local.sh

# echo "Generating hosts.config..."
rm -f $smart_dir/config/hosts.config
for i in `seq 0 $[${1}-1]`; do
    echo ${i} '127.0.0.1' `expr 1100 + ${i}`0  `expr 1100 + ${i}`1 >> $smart_dir/config/hosts.config
done

# echo "Generating system.config..."
cp $base_dir/scripts/configs/system_${1}.config $smart_dir/config/system.config

echo "Start $1 consensus nodes..."
rm -rf $base_dir/logs
mkdir $base_dir/logs
for i in `seq 0 $[${1}-1]`; do
    docker run --name smart$i --net=host --cap-add NET_ADMIN smart bash /home/runscripts/smartrun.sh bftsmart.demo.microbenchmarks.ThroughputLatencyServer $i 10 0 0 false nosig rwd > $base_dir/logs/consensus_${i}.log 2>&1 &
done

echo "Starting the sequencer..., tput:$2 kTxns/s."
$sequencer_dir/sequencer $2 &> $base_dir/logs/sequencer.log &

echo "Starting normal node..."
# for i in `seq 0 0`; do
# docker run --name normal_node$i --net=host --cap-add NET_ADMIN normal_node /normal_node/server --quiet --tps=$2 --id=$i > $base_dir/logs/normal_${i}.log 2>&1 &
# done

docker run --name normal_node0 --net=host --cap-add NET_ADMIN normal_node /normal_node/server --quiet --tps=$2 --id=0 > $base_dir/logs/normal_0.log 2>&1 &
# docker run --name normal_node0 --net=host --cap-add NET_ADMIN normal_node /normal_node/server --tps=$2 --id=0 > $base_dir/logs/normal_0.log 2>&1 &

# sleep 10

# echo "benchmarking..."
# cd $normal_node_dir
# if [ $3 == "performance" ]; then
#     go run ./cmd/client --num=100000 --org=50
# elif [ $3 == "nd" ]; then 
#     go run ./cmd/client --num=100000 --org=50 --nd=$4 
# elif [ $3 == "contention" ]; then 
#     go run ./cmd/client --num=100000 --org=50 --conflict=$4 
# elif [ $3 == "scalability" ]; then 
#     go run ./cmd/client --num=100000 --org=4
# elif [ $3 == "malicious" ]; then 
#     go run ./cmd/client --num=100000 --org=50 --order --malicious
# else 
#     echo "Invalid argument."
#     exit 1
# fi

# cd $base_dir
# echo "Please wait..."

# source $base_dir/scripts/get_data.sh
