
mkdir ../build
pushd ../build

gcc -c -ggdb ../code/main.cpp -o main.o
gcc -c -ggdb ../code/submission_queue.cpp -o submission_queue.o
gcc -pthread main.o submission_queue.o -o prog
popd
