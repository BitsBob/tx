# tx

A lightweight modal text editor written in C, inspired by the Kilo text editor tutorial and built with Vim-like keybindings.

## Building

```bash
git clone https://github.com/BitsBob/tx.git
cd tx
make
sudo cp build/tx /usr/local/bin/
```

## Configuration

tx reads `~/.txrc` on startup. Example config:

```
set tabstop               4
set syntax                true
set line_numbers          true
set status_fortune        true
set search_highlight      true
```

## Keybindings

### Normal Mode

| Key          | Action                        |
|--------------|-------------------------------|
| `i`          | Enter insert mode             |
| `v`          | Enter visual mode             |
| `h j k l`    | Move cursor                   |
| `gg` / `G`   | Jump to top / bottom          |
| `dd`         | Delete line                   |
| `dw`         | Delete word                   |
| `yy`         | Yank (copy) line              |
| `p`          | Paste                         |
| `x`          | Delete character under cursor |
| `cc`         | Change line                   |
| `cw`         | Change word                   |
| `u`          | Undo                          |
| `Ctrl+r`     | Redo                          |
| `/`          | Search                        |
| `:`          | Enter command mode            |

### Insert Mode

| Key        | Action                        |
|------------|-------------------------------|
| `Esc`      | Return to normal mode         |
| `Ctrl+s`   | Save                          |
| `({['"`    | Auto-close bracket/quote      |

### Visual Mode

| Key   | Action                |
|-------|-----------------------|
| `y`   | Yank selection        |
| `d`   | Delete selection      |
| `Esc` | Return to normal mode |

### Command Mode

| Command        | Action                                   |
|----------------|------------------------------------------|
| `:w`           | Save current buffer                      |
| `:q`           | Close buffer (exit if last)              |
| `:q!`          | Force close buffer (exit if last)        |
| `:wq`          | Save and close                           |
| `:e filename`  | Open file in new buffer                  |
| `:bn`          | Next buffer                              |
| `:bp`          | Previous buffer                          |
| `:bd`          | Close current buffer (blocks if unsaved) |
| `:bd!`         | Force close current buffer               |
| `:ls`          | List open buffers in status bar          |
| `:f` / `:find` | Search                                   |

## Syntax Highlighting

Supported languages:

- C / C++
- Python
- JavaScript / TypeScript
- Rust
- Go
- Shell
- Makefile

## Roadmap

- [x] Syntax highlighting
- [x] Delete shortcuts
- [x] Yank / paste
- [x] Undo / redo
- [x] Auto-close brackets
- [x] Multiple buffers
- [x] Window resize handling
- [x] Line numbers
- [x] LSP
- [ ] Relative line numbers
- [ ] Dot operator (`.` to repeat last action)
- [ ] Visual block mode (`Ctrl+v`)
- [ ] Pipe visual selection to shell command
- [ ] Multi-line insert
- [ ] Jump to matching bracket (`%`)
- [ ] Word motions (`w`, `b`, `e`)
- [ ] Repeat count prefix (`5j`, `3dd`)
- [ ] Mouse support

## License

MIT