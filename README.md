# Praxis 2

## Setup
compile  
`cmake -B build -DCMAKE_BUILD_TYPE=Debug`  
`make -C build`  
create pyhton virtual environment  
`python3 -m venv env`  
`source venv/bin/activate`  
install testbench  
`pip install --force-reinstall rnvs_tb-2022_projekt2_1-py3-none-any.whl`  
run tests  
`sudo rnvs-tb-dht -s .`   
