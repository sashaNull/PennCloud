
# Script to generate file:

tr -dc "A-Za-z 0-9" < /dev/urandom | fold -w100|head -n 100000 > bigbigtext.txt