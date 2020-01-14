# Socket Programming
Implementation of a connection-oriented, client-server protocol where the client sends string, and server encrypts/decrypts(Vigenère Cipher) string and returns it back to the client. 

# Instructions
Programs (client and server) can be compiled by calling “make all”

# Functions of the client
• Allows User to set IP address and port to connect to.
• When User inputs string into stdin, client sends wraps input according to the custom protocol and sends to the server.  
• When client receives reply, prints Payload to stdout.  

# Function of the server
• Handles multiple client connections.  
• Once server receives string, it encrypts/decrypts the string and returns to the client.
• Rejects connections from protocol-violating clients
• Allows User to set port  

More details: [Report](https://github.com/Shynar88/Socket-Programming/blob/master/report.pdf)
