# Wordle-Server

This code is used to simulate a Wordle Server. By following the steps, you would be able to replicate a Wordle game on your local machine.

1) Compile the wordle and wordle-main file with: gcc -Wall -Werror -g -o worlde.out wordle-main.c wordle.c -pthread
2) Compile the wordle-client file with: gcc -o wordle-client.out wordle-client.c
3) Start the server using: ./wordle.out <port-number> <seed> <dictionary-filename> <number-of-words>, an example of this using the wordle- 
   words.txt is: ./wordle.out 8190 91 wordle-words.txt 5757
4) Open another terminal and connect to the server using: ./wordle-client.out
5) Finally, start guessing the hidden Wordle word

*Note* The port number used to start the server must match the port number in wordle-client.c
