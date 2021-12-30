# Low-power wireless networking for the Internet of Things

## Members

|     Name     |  Surname  |       Username       |    MAT     |
| :----------: | :-------: | :------------------: | :--------: |
|    Carlo     | Corradini |   `carlocorradini`   | **223811** |

## Build

### Standard

```bash
$ make
```

### Debug

```bash
$ make DEBUG=true
```

### Checks

```bash
$ make CHECKS=true
```

#### Filter

> Filter only project source files

```bash
$ { make CHECKS=true 1>&2; } 2>&1 | sed '/contiki/d'
```

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details