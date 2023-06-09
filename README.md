# Weather-Data-Recorder
A simple C++ script that records the weather conditions of any city in the world by performing get http requests to external API Enspoints. It saves the most current output in a .txt file and additionally saves all entries in SQLite Database for historic analysis.

## Setup Windows 10:
VS code: Each library needs to be downloaded (Precompiled Binaries and Header files) and linked to compiler<br />
Visual Studio: Use "C++ for desktop development" and install package manager vcpkg. Example: C:\dev\vcpkg>.\vcpkg install sqlite3:x64-windows<br /><br />
Additionally to curl the code for wininet (based on windows kits sdk) is also provided
