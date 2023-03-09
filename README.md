# DTMail

Mail Server (SMTP/POP3) for developers of mail clients for local testing and developing purposes. Created with WinAPI - so you can run it only on Windows.

You're beginner developer, who decided to practice by creating mail POP3/SMTP client or just doing that as fun task? That little server will help you to test your client locally.

SMTP supported commands:
* EHLO
* MAIL FROM
* RCPT TO
* DATA
* QUIT
* AUTH LOGIN (base64)

POP3 supported commands:
* USER
* PASS
* STAT
* LIST
* QUIT
* RETR
* DELE
