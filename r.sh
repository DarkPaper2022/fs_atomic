cmake -S . -B build && cmake --build ./build
mkdir -p log
strace -f -e trace=lseek,read,write -o log/strace.txt ./build/main > ./log/runtime.txt
diff ./log/collected_data.txt ./log/standard_data.txt > ./log/diff.txt