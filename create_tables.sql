-- table for user accounts
CREATE TABLE IF NOT EXISTS "user" (
    userId INT primary key,
    userName VARCHAR(255) NOT NULL,
    passwdHash VARCHAR(255) NOT NULL, -- not shure about datetype
    userRights VARCHAR(255) NOT NULL, -- default 0 
    -- user 1
    -- moderator 2
    -- admin 3
    joinTime TIMESTAMP DEFAULT Now()
    --   userGroupLink INT
);

-- table to get group to get message 
CREATE TABLE IF NOT EXISTS userGroup (
    groupId INT primary key,
    groupName VARCHAR(255) NOT NULL,
    createTime TIMESTAMPTZ DEFAULT Now()
);

-- table to connect users to groups
CREATE TABLE IF NOT EXISTS userToGroup (
    linkId INT primary key,
    -- userId INT REFERENCES user (userId),
    -- groupId INT NOT NULL,
    userId INT references "user"(userId), 
    groupId INT references userGroup(groupId),
    joinTime TIMESTAMP DEFAULT Now(),
    adminJoined INT
);

-- table to store files from messagesк
CREATE TABLE IF NOT EXISTS filesInMessage (
    fileId INT primary key,
    inFileSystem VARCHAR(255),
    rawFile BYTEA
);

-- table to store messages
CREATE TABLE IF NOT EXISTS userMessage (
    messageId INT primary key,
    -- fromUser INT NOT NULL,
    -- toGroup INT NOT NULL, 
    messageText TEXT,
    messageTime TIMESTAMP DEFAULT Now(),
    contentType VARCHAR(255),
    fileInMessage INT references filesInMessage(fileId),
    
    userId INT references "user"(userId),
    droupId INT references userGroup(groupId),
    linkId INT references userToGroup(linkId) 
);

-- table to store actives and prices
CREATE TABLE IF NOT EXISTS "active" (
    activeId INT primary key,
    activeName VARCHAR(255), 
    activeTicker VARCHAR(16), -- TODO: check size
    activePrice INT,
    activeDescription TEXT
    -- something else?
);

CREATE TABLE IF NOT EXISTS usersActives (
    lineId INT primary key,
    userId INT references "user"(userId), 
    activeId INT references "active"(activeId),
    ammount INT,
    avgBoughtPrice FLOAT(5) -- TODO: check size
);

CREATE TABLE IF NOT EXISTS "transaction" (
    transactionId INT primary key,
    lineId INT references usersActives(lineId),
    transactionTime TIMESTAMP DEFAULT Now(),
    transactionPrice FLOAT(10) -- TODO: check size
)