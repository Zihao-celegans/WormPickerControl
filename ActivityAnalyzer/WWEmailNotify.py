
import smtplib, ssl

port = 465      # for SSL
smtp_server = "smtp.gmail.com"

sender_email = "ImageGrabberAlerts@gmail.com"
password = "my cows are green5 yes"
receiver_email = "jeonginn@seas.upenn.edu"
message = """\
Subject: Hi there

This message is sent from Python."""

# Creating a secure SSL context
context = ssl.create_default_context()

with smtplib.SMTP_SSL("smtp.gmail.com", port, context=context) as server:
    server.login("ImageGrabberAlerts@gmail.com", password)
    # send email here
    server.sendmail(sender_email, receiver_email, message)