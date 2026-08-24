(
  # Send 10-byte raw TCP fragments with 1s delays
  printf "POST /post" ; sleep 1
  printf " HTTP/1.1\r" ; sleep 1
  printf "\nHost: htt" ; sleep 1
  printf "pbin.org\r\n" ; sleep 1
  printf "Content-Ty" ; sleep 1
  printf "pe: text/p" ; sleep 1
  printf "lain\r\nCon" ; sleep 1
  printf "tent-Lengt" ; sleep 1
  printf "h: 17\r\n\r\n" ; sleep 1

  # Send body chunks (17 total bytes)
  printf "Hello World!" ; sleep 1  # 12 bytes
  printf "12345"                   # 5 bytes
) | nc localhost 9000
