Decentralized Data Storage Simulator (C++)
📌 Overview

  This project is a C++ simulator that demonstrates the concept of decentralized data storage. Instead of storing all data in a single centralized location, the system distributes it across multiple regions and nodes using a hashing-based routing mechanism.
  
  It is designed for learning purposes, to showcase how decentralized systems manage data distribution, retrieval, and routing.

🚀 Features

  Multi-Region Architecture
  Data is distributed across different regions (e.g., Delhi, Mumbai, Bangalore).
  
  Multi-Node Storage
  Each region consists of multiple nodes, simulating partitioning/sharding.
  
  Double Hashing Function
  Uses a custom hash function for consistent key generation and data placement.
  
  Deterministic Routing
  The same (username + email) always maps to the same region and node.
  
  Data Retrieval
  Queries are routed to the correct region/node to fetch stored information.

🛠️ Tech Stack

  Language: C++
  
  Data Structures: unordered_map, vector
  
  Concepts Used: Hashing, Data Partitioning, Sharding, Decentralization

📖 Example Workflow

  Add Data
  
  User credentials (username, email, social ID, password) are hashed.
  
  Global Controller routes the data to a specific region.
  
  Inside the region, data is assigned to a node.
  
  Retrieve Data
  
  The same (username, email) is hashed again.
  
  Global Controller redirects the query to the correct region + node.
  
  Data is retrieved if available.

🔮 Future Improvements

  Add replication for fault tolerance.
  
  Replace the Global Controller with a Hash Ring for true decentralization.
  
  Implement network-based communication between nodes.
  
  Add a CLI interface for user interaction.

📷 Demo Output
  GlobalController routed data to region: Delhi
  Node 1 stored data at key 13847477
  Node 0 stored data at key 98472721
  
  --- Retrieving Data ---
  Data found in region Delhi:
  User : Gurkirat
     Social ID: SOCIAL123
     Password : pass123
  Data not found!

🎯 Learning Outcome

  Understood the basics of decentralized storage systems.
  
  Implemented hash-based routing and multi-node distribution.
  
  Strengthened knowledge of distributed systems, partitioning, and hashing techniques.
