(
  # --- FRAGMENTED HEADERS ---
  printf "POST /post" ; sleep 1
  printf " HTTP/1.1\r" ; sleep 1
  printf "\nHost: local" ; sleep 1
  printf "host:9000\r" ; sleep 1
  printf "\nContent-Ty" ; sleep 1
  printf "pe: text/p" ; sleep 1
  printf "lain\r\nTrans" ; sleep 1
  printf "fer-Encodi" ; sleep 1
  printf "ng: chunke" ; sleep 1
  printf "d\r\n\r\n" ; sleep 1

  # --- CHUNKED BODY ---
  # Chunk 1: "Hello World!" is 12 bytes -> hex 'c'
  printf "c\r\nHello World!\r\n" ; sleep 1

  # Chunk 2: "12345" is 5 bytes -> hex '5'
  printf "5\r\n12345\r\n" ; sleep 1

  # Final terminating chunk (0 length)
  printf "0\r\n\r\n"
) | nc localhost 9000
