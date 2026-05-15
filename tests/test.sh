#!/bin/bash

BASE="http://localhost:8080"
BASE2="http://localhost:8081"

echo "================ BASIC GET ================"

curl -v $BASE/
curl -v $BASE/index.html
curl -v $BASE/cette-page-nexiste-pas
curl -v $BASE/errors/404.html

echo "================ REDIRECT ================"

curl -v $BASE/redirect
curl -v -L $BASE/redirect
curl -I $BASE/redirect

echo "================ POST ================"

curl -v -X POST $BASE/uploads/mon_fichier.txt -H "Content-Type: text/plain" -d "Contenu de mon fichier de test"

curl -v $BASE/uploads/mon_fichier.txt
curl -v -O $BASE/uploads/mon_fichier.txt

curl -v -X POST $BASE/uploads/size10 -d "0123456789"
curl -v -X POST $BASE/uploads/size11 -d "00123456789"

curl -v -X POST $BASE/uploads/hello \
-H "Content-Type: text/plain" \
-H 'Content-Disposition: attachment; filename="@hello.txt"' \
-d "@./test_files/hello.txt"

curl -v -X POST $BASE/uploads/image.png \
-H "Content-Type: image/png" \
--data-binary "@./www/index/42-logo.png"

echo "================ DELETE ================"

curl -v -X DELETE $BASE/uploads/mon_fichier.txt
curl -v $BASE/uploads/mon_fichier.txt
curl -v -X DELETE $BASE/uploads/nexiste_pas.txt
curl -v -X DELETE $BASE/index.html

echo "================ AUTOINDEX ================"

curl -v $BASE/list

echo "================ CGI ================"

curl -v $BASE/cgi-bin/test.py
curl -v "$BASE/cgi-bin/test.py?name=webserv&lang=cpp"
curl -v -X POST $BASE/cgi-bin/test.py \
-H "Content-Type: application/x-www-form-urlencoded" \
-d "user=test&action=login"

curl -v $BASE/cgi-bin/inexistant.py

echo "================ NOT ALLOWED METHOD ================"

curl -v -X DELETE $BASE/index.html

echo "================ CLIENT BODY SIZE ================"

python3 -c "import sys; sys.stdout.buffer.write(b'X' * (1024 * 1024 * 6))" > /tmp/bigbody.bin

curl -v -X POST $BASE \
-H "Content-Type: application/octet-stream" \
--data-binary @/tmp/bigbody.bin

rm -f /tmp/bigbody.bin

curl -v -X POST $BASE/cgi-bin/another_test.py \
-H "Content-Type: application/x-www-form-urlencoded" \
-d "data=ok"

echo "================ CONCURRENCY ================"

curl -s -w "CGI: %{time_total}s\n" $BASE/cgi-bin/test.py &
curl -s -w "Static: %{time_total}s\n" $BASE/ &
wait

for i in 1 2 3 4 5; do
  curl -s -w "CGI $i: %{time_total}s\n" -o /dev/null "$BASE/cgi-bin/another_test.py?id=$i" &
done
wait

echo "================ EDGE CASES ================"

curl -v "http://localhost:8080/$(python3 -c "print('a'*5000)")"

curl -v -X POST $BASE/uploads/empty.txt \
-H "Content-Type: text/plain" \
-d ""

curl -v $BASE//index.html

echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

echo "================ MULTIPLE SERVERS ================"

curl -v $BASE2/
curl -v -X POST $BASE2/oui -d "data"

echo "================ UNKNOWN METHOD ================"

curl -v -X PROPFIND $BASE/
curl -v -X FAKEMETHOD $BASE/index.html

echo "================ DUPLICATE PORT ================"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
$SCRIPT_DIR/../webserv conf/default.conf &
DUP_PID=$!
sleep 1
if kill -0 $DUP_PID 2>/dev/null; then
    echo "FAIL: second server started on occupied port"
    kill $DUP_PID
else
    echo "PASS: second server failed to bind as expected"
fi
curl -v $BASE/

echo "================ BASIC TEST FINISHED =========="