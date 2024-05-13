# 505 Project Team T15

## Team Members
- Adwait Agashe
- Emma Jin
- Muskaan Beriwal
- Sasha Nguyen

## Project Information

This project encompasses a distributed system architecture with multiple interconnected components. At its core, the backend utilizes distributed storage nodes to store and replicate data, ensuring fault tolerance and scalability.

### Key Features:
- **Distributed Storage**: Efficient data storage and replication across multiple nodes.
- **Frontend Interface**: User-friendly interface supporting various HTTP requests and handling user authentication.
- **User Management**: Comprehensive user account management, including sign-up and password change functionalities.
- **Webmail Service**: Robust email handling for both incoming and outgoing emails.
- **Web Storage Service**: Facilitates file management with support for nested folders and diverse file types.
- **Admin Console**: Provides insights into the system’s status and enables control over individual storage nodes for testing fault tolerance and recovery.
- **Bulletin Board Feature**: Potential for collaborative interaction among users.

## How to Run

### Starting the System

From the main directory:

```bash
./start_system.sh 9 3
```

- **9**: Number of backend servers.
- **3**: Number of frontend servers.

### Stopping the System

From the main directory:

```bash
./stop_system.sh
```

### Resetting Data

From the main directory:

```bash
./reset_data.sh
```

### Starting or Stopping Components Individually

1. **Navigate to the component's directory**:
   
   ```bash
   cd path/to/component_directory
   ```

2. **Starting a Component**:

   ```bash
   ./start_component.sh
   ```

3. **Stopping a Component**:

   ```bash
   ./stop_component.sh
   ```

## Component Images

1. **Image 1**: ![PennCloud](https://iili.io/J6uzXF1.png)
2. **Image 2**: ![Email Page](https://iili.io/J6uzj8g.png)
3. **Image 3**: ![Drive Page](https://iili.io/J6uz4cX.png)
4. **Image 4**: ![Admin Page](https://iili.io/J6uzhcF.png)
5. **Image 5**: ![Bulletin Board Page](https://iili.io/J6uzrFt.png)
