# Multichat program in C++ with a multithreaded server and client

## NOTES:  

This is an experiment to implement a proper working client-server architecture, in C++.

Feel free to try it out yourself. Feedback about bugs, possible improvements, etc. are greatly appreciated.

It should not be used, as it is, in any kind of professional software as it is still in development. 

I'm working on a report to make it more understandable to people who want to learn how it is structured

You can reach me at "helmer.n@hotmail.com".

---------------------------------

## DOWNLOAD STUFF:   

1. Download Microsoft Visual Studio 2015 Community:

You can find Visual Studio 2015 from here: 
https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx
and then choose "Download Community Free".

NOTE: I have only tried this in Visual Studio 2015 and I cannot confirm if the code works
      in older versions of Visual Studio. The reason its best to do in visual studio is because there
      are Microsoft-supplied technology (in this case sockets) used here.
      

2. Download the C++ boost library:

Download boost from here:
http://www.boost.org/users/history/
The version used in this implementation is 1.60.0. 

I see that the Thread library is updated in 1.60.0,
so it's possible this code doesn't work with an earlier version of boost.

Instructions how to build the library should be available on the website

------------------------------

## RUN THE PROGRAMS:

### Client

1. Create a project in Visual Studio called Client. 



2. Put the code in Client.cpp in the Client project and compile.



3. To run Client you need to run it with the IP address of the server. 
   This can be done using command line or through the IDE.
   
   1. Command line:
        * Run the following command: `start "" <path to Client.exe> <IP address of the server>`.
        
   2. Visual Studio:
        1. Go to "Project" -> "Properties".
        
        2. Under "Configuration Properties", click "Debugging".
        
        3. In "Command Arguments", type in the IP address.
        
        4. Then run the program


### Server

1. Create a project in Visual Studio called Server.

2. Put the code in Server.cpp in the server project and compile.

3. Server doesn't require any arguments.

      1. Command line:
            * Run the following command: `start "" <path to Server.exe>`.

### Start chatting

* To make them communicate, type something in a client and hit enter.

* The message should appear in server. 
   
* When several clients are running and you enter a message in one client, the server should receive that message,
  and then all other clients should receive that message and also info about which client sent it.
   
###  NOTE: You don't need several computers to test several clients. You can run several instances of Client and one Server on  the same machine. 
