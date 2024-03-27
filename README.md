# dsv
distributed system variable

# dependencies
ubuntu
sudo apt-get update
sudo apt-get install libcjson-dev
sudo apt-get install libzmq-dev
sudo apt-get install libczmq-dev

# To test it
git clone git@github.com:denghui01/dsv.git
cd dsv
mkdir build && cd build
cmake ..
make

./dsv_server/dsv_server

open another terminal
./dsv/sv -c -i 123 -f ./dsvs.json
./dsv/sv set [123]/SYS/TEST/U32 123
./dsv/sv get /SYS/TEST/U32
./dsv/sv save

restart dsv_server
./dsv/sv restore

./dsv/sv track enable /TEST/
./dsv/sv track disable /TEST/

./devman/devman -vv -f ./dev_dsvs.json