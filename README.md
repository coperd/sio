- vim: textwidth=80

# sio
Simple IO bench

===

`$ make sio`
`$ ./sio -h` for help information


  * currently only support `O_DIRECT` mode to opearte on drives
  * `--warmup xx`  is used for writing xx blocks (4k) from the beginning of drive


**TODOs**

  * ~~add flexbile block size support~~
  * add different working mode
  * ~~add flexible way to get disk size (without introducing extra r/w operations 
          inside qemu), currently 512MB is hardcoded (only for our SSD
              testing).~~
