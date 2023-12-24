#!/bin/sh

groff -t -e -mandoc -Tascii guasi.3 | col -bx > guasi.txt
groff -t -e -mandoc -Tps guasi.3 | ps2pdf - guasi.pdf
man2html < guasi.3 | sed 's/<BODY>/<BODY text="#0000FF" bgcolor="#FFFFFF" style="font-family: monospace;">/i' > guasi.html

