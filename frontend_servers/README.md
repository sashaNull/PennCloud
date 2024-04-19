TODO: 

1. Initials: 
    - Add support for redirection to login page when we hit the root url "127.0.0.1:8000/"
    - Add functionality to talk to the coordinator while talking to the backend.

2. Cookie support
    - If user is not logged in, any other pages except login and signup (home, drive, email) to the login page. 
    - Store cookies and fetch user info based on the cookie.
    - If user is already logged in then redirect from login or signup page to the home page
    - cookiename "username" adwait

3. Logout functionality --> I think we would only need to delete cookie and redirect to login but check

4. Email 
    - Actual HTML code for the Email
    - Frontend talks to SMTP server when sending emails
    - Frontend talks to backend when fetching emails
    - SMTP talks to frontend and backend to store data

5. Drive
    - Actual HTML for the drive
    - Talk to the backend to fetch the data
    - Upload, Delete, Download, Move, Rename --> files and folders.

6. Admin Page:
    - Shows the servers that are currently active
    - Functionlaity to start and stop frontend and backend servers
    - View raw data from the backend server and display

7. Load Balancer:
    - The browser talks to this and it redirects to one of the active frontend servers
    - Heartbeat functionality --> Similar to the backend coordinator node.
