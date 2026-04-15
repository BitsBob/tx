# tx
Small text editor in C, built off the Kilo text editor tutorial.

# Building Yourself
```bash
git clone https://github.com/BitsBob/tx.git
cd tx
make
sudo cp build/tx /usr/local/bin/
```

| Mode    | Key           | Action                                 |
|---------|---------------|----------------------------------------|
| Normal  | i             | enter insert mode                      |
| Normal  | v             | enter visual mode                      |
| Normal  | dd            | delete line                            |
| Normal  | gg / G        | top / bottom                           |
| Normal  | /             | search                                 |
| Command | :w :q :wq :q! | write / quit / write+quit / force quit |
| Command | :open         | open file                              |

# ! TODO
 - [ ] Undo/Redo
 - [x] Syntax Highlighting
 - [x] Delete Shortcuts
 - [ ] Line Numbers (Relative, Actual)
 - [ ] Dot Operator
 - [ ] Visual Block Mode
 - [ ] Visual Mode pipe to shell command
 - [ ] Multi Line insert
 - [x] Text Objects (Sort of)

# License
MIT
