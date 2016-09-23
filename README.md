NOTES:  

This is an experiment to implement a proper working client-server architecture, in C++.

Feel free to try it out yourself. Feedback about bugs, possible improvements, etc. are greatly appreciated.

It should not be used, as it is, in any kind of professional software as it is still in development. 

I'm working on a report to make it more understandable to people who want to learn how it is structured

---------------------------------

DOWNLOAD STUFF:   

1. Download Microsoft Visual Studio 2015 Community:

You can find Visual Studio 2015 from here: 
https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx
and then choose "Download Community Free".

NOTE: I have only tried this in Visual Studio 2015 and I cannot confirm if the code works
      in older versions of Visual Studio. The reason its best to do in visual studio is because there
      are Microsoft-supplied technology (in this case sockets) used here.
      
------------------------------      

2. Download the C++ boost library:

Download boost from here:
http://www.boost.org/users/history/
The version used in this implementation is 1.60.0. 

The only boost library that I'm using is Thread. I see that this is updated in 1.60.0,
so it's possible this code doesn't work with an earlier version of boost.

I'm only using a static library "thread.hpp", so there's no need to build anything.

------------------------------

RUN THE PROGRAMS:

1. Create two projects in Visual Studio. One called "Server" and another called "Client" 
   (or whatever you like, but I'll refer to them as "Server" and "Client" from this point).

----------------------------------

2. Put the code in Server.cpp in the Server project and Client.cpp in the Client project and compile both.

----------------------------------

3. To run a Client you need to run it with the IP address of the server. 
   This can be done using command line or through the IDE.
   
   Command line:
        Run the following command: start "" "path to Client.exe" "IP address of the server".
        
   Visual Studio:
        Go to "Project" -> "Properties".
        
        Under "Configuration Properties", click "Debugging".
        
        In "Command Arguments", type in the IP address.

----------------------------------

4. Run Server like normal. No arguments.

----------------------------------

5. To make them communicate, type something in a client and hit enter.

   The message should appear in server. 
   
   When several clients are running and you enter a message in one client, the server should receive that message,
   and then all other clients should receive that message and also info about which client sent it.
   
   If this happens, the programs work correctly.
   
   NOTE: You don't need several computers to test several clients. You can run several clients and the server on the same machine. 
