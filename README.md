# Praxis 2

## Setup
create pyhton virtual environment  
`python3 -m venv venv`  
`source venv/bin/activate`  

install testbench  
`pip install --force-reinstall rnvs_tb-2022_projekt3_1-py3-none-any.whl `  

## Run tests
compile project  
`cmake -B build -DCMAKE_BUILD_TYPE=Debug`  
`make -C build`

run tests  
`sudo rnvs-tb-dht -s .`

## Manual Testing
`./peer 127.0.0.1 Port [ID] [127.0.0.1 Peer-Port]`

ID: ID of Peer. If not set: 0.  
Peer-Port: port of existing peer in network. If not set:  Peer is first one.
