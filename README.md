# Exam Result Ranking System (DSA Mini Project)

A website with a **C backend** (its own tiny HTTP server, no libraries) and an **HTML/CSS/JavaScript frontend**.

## Run
**Windows:** install MinGW gcc (or use Dev-C++ / Code::Blocks, which include it), then double-click **build.bat**.
Manual: `gcc server.c -o server.exe -lws2_32` then `server.exe`.
Visual Studio: open "Developer Command Prompt", run `cl server.c`, then `server.exe`.

**Linux / Mac:** `sh run.sh` (or `gcc server.c -o server && ./server`).

Open the address the program prints (normally **http://localhost:8080**; if 8080 is busy it uses 8081, 8082...).
Run it from inside this folder (it must see the `public` folder and `students.txt`).

## Structure
```
server.c        C backend: REST API + static file server
build.bat       Windows: compile and run in one click
run.sh          Linux/Mac: compile and run
students.txt    Data file (saved automatically, sample data included)
public/         Frontend: index.html, style.css, app.js
```

## API
| Method | URL | Purpose |
|---|---|---|
| GET | /api/students?sort=rank\|roll\|name | Ranked list (JSON) |
| POST | /api/students | Add student (roll, name, m1..m5) |
| PUT | /api/students | Update student |
| DELETE | /api/students?roll=N | Delete student |
| GET | /api/search?roll=N | Binary search by roll number |

## DSA concepts used (for viva)
- **Array of structs** stores students (`Student db[1000]`).
- **Merge sort** O(n log n), stable, sorts by total marks (desc), roll, or name.
- **Ranking with ties**: equal totals share a rank (e.g. 1, 2, 2, 4).
- **Binary search** O(log n) on the roll-sorted array; the UI shows the number of steps.
- **File handling**: data persisted in `students.txt`.
- **Pass rule**: a student fails if any subject is below 35 (change `PASS_MARK` in `public/app.js`).
