# cocos2d-x-gd

Recreating rob's modifications to cocos2d-x v2.2.3.

The idea of this project is to be able to insert our own compiled libcocos2d.dll into the GeometryDash folder and load the game fine.
When told to resolve something, look up the relevant code of this project (the libcocos2d.dll source code), then use x64dbg and ida mcps accordingly. 
The idea is to be 100% matching with robtop's original libcocos2d.dll, which you can reverse engineer using IDA MCP 

You are NOT allowed to add any functions, only modify existing ones unless explicitly told to work on adding new functions.


# Build commands:

cmake --build build