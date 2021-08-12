**What is shmhash?**

ShmHash is a C++ key-value store implemented with multi-level hash table based on linux shared memory. 
Using shmhash, one process can store kv data in shmhash so that multiple processes can read the data without locking.(But also note that shmhash is not write-write lock free, if multiple processes are writing to shmhash, one hash to enforece a lock mechanism by himself.)

**Features**

 - 1. Built-in cache replacement mechanism.
 - 2. Store most data at level 1 hashtable to achieve O(1) performance, while the other levels's size degrade very quickly.
 - 3. Use pre-recycle pool to avoid read-write colision, so that multiple reader processes can read at the same time while the writer process is writing the same key.

**Memory structure**

![image](https://user-images.githubusercontent.com/16682224/129141214-44455543-5f3c-43c9-9d8f-c4237f9f715d.png)
