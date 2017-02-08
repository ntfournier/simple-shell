# Simple Shell

## How to compile and run

```
make
./shell
```


## Background tasks

```
./shell
$>sleep 800 &       # Run sleep in background &
$>btasks            # List background tasks
  [1] Sleep
$>exit
  [1] Sleep
Could not exit still some tasks running # Prevent exiting when tasks running in background.
```

