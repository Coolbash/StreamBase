This is a test StreamBase project. 
The technical assignment is here: https://gist.github.com/osamakhn/a20088c2cc45447ecd941ca121a8358b

The C++17 language standard version is used.

The project contains from two applications: server and client.
A couple of files are common for both projects.

For keeping a handle to a pipe, correctly closing it on destruction and making low-level communication with the pipe, there is a class CNamedPipeClient (and CNamedPipeServer), that is common for client and server projects. This class has disabled copy-constructor and releases the handle on destruction.

How server works:
It creates a pipe (the name of the pipe declared in common file global_types.h ) and waits for connection. After connection is made, it creates a thread for communicating through the pipe. The thread then creates a remote client object. And the main thread creates another pipe and waits for connection.
There is a static number of waiting pipes int the program. When another pipe waits for communication, the number increases. After connection, the number decreases.
After client disconnection, worker thread releases all data related to the client and waits for another connection.
The main thread won’t create any pipes if there is another thread with a pipe that is waiting for connection.
Also, the main thread won’t create any pipes if the maximum allowed connection quantity is reached. (it’s also declared in global_types.h). 
For correct closing all connections, the program sets a handler for Ctrl-C, Ctrl-F4 keyboard shortcuts and other closing event (closing the console window or shutting down the system).

How client works:
The main function just creates an instance of the Client class that keeps all the stuff for piping and makes all communication.
At first it compares the protocol versions, then exchanges some simple data and then creates remote objects and calls their methods.

When a client asks a server to create an object, the object is created at server side and kept in CRemoteClient class. Each created remotely object has unique number that is used for identifying the object in communication. It’s just a static counter (protected for multithreading incrementation) that increases in remote object constructor. When the client disconnects, servers destroys the CRemoteClient object and all objects, that were created by the client and weren’t destroyed, destroys with the CRemoteClient object.

In each message there is a message type identifier. In simple cases, when a message with the identifier is enough, the messages consist only the identifier. If a message must consist some data, the data follows the identifier. Size of the message and message structure depends on identifier. Simple data like simple types and concrete structures just follows the identifier. Containers like strings and vectors have size first and then the data.
For creating an object, after identifier “create object” follows type of the object. In respond serves sends the ID of created object.
For calling a method of the object, client send with request the ID of the object and (optionally) parameter data. Server responds with the result.


I haven’t created asynchronous sending of requests in the client due to a couple of reasons. 
First: windows named pipes API is very old and has mechanism for working asynchronously (overlapped IO), but a pipe must work in one mode sync or async, not both. Of course, it’s possible to open two pipes: one for sync and one for async operations. But then we should create some mechanism on server that can aggregate several connections on one client.
The second reason is complexity and performance. What for we need both sync and async operations? It’s possible to achieve with multithreading, but then we have a lot of problems: If we allow sending to one pipe from different threads, then we need to protect those operations with mutexes or something. And basically, make all operations single-threaded (there is no way to improve performance here). But then we get all problems with request-response order. It might not be a problem but when we suddenly need a sync operation (a transaction with corresponding response), we can’t guarantee that the response will correspond the request and won’t be a response for some other thread or async request. Of course, it’s also doable but with much more effort: with synchronization on both ends. 

