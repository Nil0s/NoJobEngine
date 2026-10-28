"""Fetch the pinned official WinSparkle SDK; never publish artifacts."""
from pathlib import Path
import hashlib, io, urllib.request, zipfile
URL = "https://github.com/vslavik/winsparkle/releases/download/v0.9.4/WinSparkle-0.9.4.zip"
SHA256 = "6037df37fc263bd1650a1c4949681a9d40ffe991d01f35892a406cb5d103c976"
def main():
    """Verify the archive before extracting SDK files beneath out/tooling."""
    root = Path(__file__).resolve().parents[2] / "out/tooling"
    data = urllib.request.urlopen(URL, timeout=90).read()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise RuntimeError("WinSparkle SDK checksum mismatch.")
    root.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for member in archive.infolist():
            if not (root / member.filename).resolve().is_relative_to(root.resolve()):
                raise RuntimeError("Invalid SDK archive path.")
        archive.extractall(root)
    print(root / "WinSparkle-0.9.4")
if __name__ == "__main__":
    main()
