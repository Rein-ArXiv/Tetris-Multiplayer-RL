import subprocess
import sys


def _terminate_direct_child(proc):
    """Terminate the direct child, escalating to kill after a 5s grace period."""
    if proc.poll() is not None:
        return
    try:
        proc.terminate()
    except ProcessLookupError:
        proc.wait()
        return
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def run_logged(argv, *, cwd, log_path, env=None):
    """Run `argv` as a single direct child, mirroring merged output to terminal and log.

    stdout and stderr are merged and streamed line by line to sys.stdout and to
    log_path, flushing after every line. The command runs without a shell. The
    log is opened exclusively ('x') before launch, so an existing file is never
    overwritten. A non-zero exit status raises subprocess.CalledProcessError;
    0 is returned only on genuine success.

    If reading, printing, or writing fails or is interrupted while the child is
    still alive, the child is terminated, then killed after a 5s timeout. The
    child's stdout is always closed; the log file is always closed.

    Scope and limitations:
    * Only the direct child is managed; processes it spawns are not terminated
      as a process group.
    * The caller is responsible for ensuring log_path's parent directory exists
      and is writable.
    * There is no fsync/power-loss durability guarantee for the log file.
    * When env is None the child inherits the parent environment; this helper does not dump environment values; child output is recorded.
    """
    if not isinstance(argv, list) or not argv or not all(isinstance(a, str) for a in argv):
        raise ValueError("argv must be a nonempty list of strings")

    log_file = open(log_path, "x", encoding="utf-8", errors="replace")
    try:
        proc = subprocess.Popen(
            argv,
            cwd=cwd,
            env=env,
            shell=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        try:
            for line in proc.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log_file.write(line)
                log_file.flush()
            returncode = proc.wait()
        except BaseException:
            _terminate_direct_child(proc)
            raise
        finally:
            proc.stdout.close()
    finally:
        log_file.close()

    if returncode != 0:
        raise subprocess.CalledProcessError(returncode, argv)
    return 0
