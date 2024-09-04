-- table for user accounts
CREATE TABLE IF NOT EXISTS user (
    userId INT GENERATED ALWAYS AS IDENTITY,
    userName VARCHAR(255) NOT NULL,
    passwdHash VARCHAR(255) NOT NULL, -- not shure about datetype
    userRights VARCHAR(255) NOT NULL, -- default 0 
    -- user 1
    -- moderator 2
    -- admin 3
    joinTime TIMESTAMPTZ DEFAULT Now()
    --   userGroupLink INT
);

-- table to connect users to groups
CREATE TABLE IF NOT EXISTS userToGroup (
    linkId INT GENERATED ALWAYS AS IDENTITY,
    -- userId INT REFERENCES user (userId),
    -- groupId INT NOT NULL,
    CONSTRAINT userId FOREIGN KEY(user) REFERENCES user(userId),
    CONSTRAINT groupId FOREIGN KEY(userGroup) REFERENCES userGroup(groupId),
    joinTime TIMESTAMPTZ DEFAULT Now()
    adminJoined INT
);

-- table to get group to get message 
CREATE TABLE IF NOT EXISTS userGroup (
    groupId INT GENERATED ALWAYS AS IDENTITY,
    groupName VARCHAR(255) NOT NULL,
    createTime TIMESTAMPTZ DEFAULT Now()
);

-- table to store messages
CREATE TABLE IF NOT EXISTS userMessage (
    messageId INT GENERATED ALWAYS AS IDENTITY,
    -- fromUser INT NOT NULL,
    -- toGroup INT NOT NULL, 
    CONSTRAINT fromUser FOREIGN KEY(user) REFERENCES user(userId),
    CONSTRAINT toGroup FOREIGN KEY(userGroup) REFERENCES userGroup(groupId),
    messageText TEXT,
    messageTime TIMESTAMPTZ DEFAULT Now(),
    contentType VARCHAR(255),
    CONSTRAINT fileId FOREIGN KEY(filesInMessage) REFERENCES filesInMessage(fileId) DEFAULT NULL    
);

-- table to store files from messagesк
CREATE TABLE IF NOT EXISTS filesInMessage ( 
    fileId INT GENERATED ALWAYS AS IDENTITY,
    inFileSystem VARCHAR(255),
    rawFile BYTEA.
);

-- table to store actives and prices
CREATE TABLE IF NOT EXISTS active (
    activeId INT GENERATED ALWAYS IS IDENTITY,
    activeName VARCHAR(255), 
    activeTicker VARCHAR(16), -- TODO: check size
    activePrice INT,
    activeDescription TEXT
    -- something else?
)

CREATE TABLE IF NOT EXISTS usersActives (
    lineId INT GENERATED ALWAYS IS IDENTITY,
    CONSTRAINT user FOREIGN KEY(user) REFERENCES user(userId),
    CONSTRAINT active FOREIGN KEY(active) REFERENCES active(activeId),
    ammount INT,
    avgBoughtPrice FLOAT(5) -- TODO: check size
)

CREATE TABLE IF NOT EXISTS transaction (
    transactionId INT GENERATED ALWAYS AS IDENTITY,
    CONSTRAINT usersActiveId FOREIGN KEY(usersActives) REFERENCES usersActives(lineId),
    transactionTime TIMESTAMPTZ DEFAULT Now(),
    transactionPrice FLOAT(10) -- TODO: check size
)