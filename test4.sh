# Generate a large file (assuming your limit is 1MB)
dd if=/dev/urandom of=/tmp/bigbody.bin bs=1K count=999

curl -v -X POST http://localhost:9000/upload \
  -H "Content-Type: application/octet-stream" \
  --data-binary @/tmp/bigbody.bin
