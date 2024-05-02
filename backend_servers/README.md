To run a server:
./server -v server_config.txt 0

TODO:
1. Cannot hardcode values in get_file_name 
    Make the tablet names more spaced out like aa-ag, ah-am, etc.
    Include more servers
    Make a script to run all servers at the same time.

Requirements to implement this:
- Modify initialise_cache function so it reads from config file instead of the data directory.
- Modify the get_file_name function so that it works with variable file names.
Planned Deadline: 19th April
-------------------------------------------------------------------------
2. Primary Based Replication:
- Coordinator randomly decides a primary server for each tablet.
- Coordinator returns primary for writes and any for reads.
- In backend, If recv message from a frontend, if a get do nothing else forward to all servers having this row.
              If recv from backend, perform operation and return quit.
- If primary goes down, coordinator assigns a new primary.
Note: Change F2B message to add another variable to know if from frontend or backend.
Planned Deadline: 22nd April

Method:
Coordinator
- Change the range to be dynamic inside coordinator.
- Modify the coordinator to randomly choose a primary if the current primary is down on heartbeat.
- If the currently primary is alive, dont change anything.
- If coordinator has a get request, forward to any random active server for that tablet
- If it has a put, delete or cput request, it forwards it to the primary.

Backend Server:
- It needs to know which servers have what tablets.
- If it gets a message from the frontend server, if it is not a get, it forwards it to all the backend servers having this tablet.
- If it gets a message from a backend server, it only does the operation and replies success
- After all servers reply with success, the current server gives the response.
-------------------------------------------------------------------------
3. Distributed Checkpointing and Recovery:
- Add version numbers to the tablets every time we checkpoint.
- On startup when we want to recover, it will ask the primary of each tablet for its checkpoint version number and log file length for that tablet.
- If any of them (version number or length of log file) is different, download the checkpoint file and log file from primary.
- After this is done, replay the log for each tablet.
Planned Deadline: 25th April
-------------------------------------------------------------------------
4. Coordinator chooses servers based on statistics --> Number of connections, etc.

5. Locking on a per row basis.

6. LRU cache (Maybe not).

7. Read and Write locks different.

8. Coordinator failure handling.