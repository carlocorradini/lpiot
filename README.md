# Low-power wireless networking for the Internet of Things

## Members

|     Name     |  Surname  |       Username       |    MAT     |
| :----------: | :-------: | :------------------: | :--------: |
|    Carlo     | Corradini |   `carlocorradini`   | **223811** |

## Build

```bash
$ make
```

### Zolertia Firefly

> Testbed

```bash
$ make TARGET=zoul
```

### Statistics

> Enable statistics output used by the analisy script

```bash
$ make STATS=true
```

## Recipes

> Default recipe is *building/compiling*

### cleanall

> Remove generated files, directories and logs

```
$ make cleanall
```

## Analisy

> Build with statistics: ```make STATS=true```

### Cooja

```bash
$ python scenarios/parse-stats.py <FILE_NAME>.log
```

### Testbed

```bash
$ python scenarios/parse-stats.py scenarios/testbed/<JOB_ID_FOLDER>/test.log --testbed
```

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details
