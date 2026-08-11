#!/usr/bin/env bash
#
# Build statically linked Linux binaries for deployment.
#
#   scripts/build-release.sh              # both architectures
#   scripts/build-release.sh x64          # just one
#   DIST_DIR=/somewhere scripts/build-release.sh
#
# Output lands in dist/dahlia-linux-<arch>, spelling the architecture Node's way
# (x64, arm64) rather than Docker's (amd64, arm64) -- the website resolves the
# binary by `process.arch` at startup, so the file name is what makes that work.
#
# Nothing here runs Dahlia in a container. The image is only borrowing a Linux
# toolchain, since this is normally invoked from macOS; see
# docker/Dockerfile.release for what the build actually does.

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
dist="${DIST_DIR:-$repo_root/dist}"

arches=("$@")
if [ ${#arches[@]} -eq 0 ]; then
	arches=(x64 arm64)
fi

docker_platform() {
	case "$1" in
		x64) echo linux/amd64 ;;
		arm64) echo linux/arm64 ;;
		*)
			echo "unknown architecture '$1' (expected x64 or arm64)" >&2
			return 1
			;;
	esac
}

staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT

mkdir -p "$dist"

for arch in "${arches[@]}"; do
	platform=$(docker_platform "$arch")

	echo "==> $platform"
	# Building a foreign architecture goes through QEMU, which Docker Desktop
	# registers for you. It is slow -- minutes, not seconds -- but this is a
	# handful of translation units and it only runs when Dahlia changes.
	docker buildx build \
		--platform "$platform" \
		--file "$repo_root/docker/Dockerfile.release" \
		--target export \
		--output "type=local,dest=$staging" \
		"$repo_root"

	out="$dist/dahlia-linux-$arch"
	install -m 755 "$staging/dahlia" "$out"
	rm -f "$staging/dahlia"

	echo "==> $out"
	# Print what shipped, so the thing on the instance can be identified later.
	shasum -a 256 "$out" 2>/dev/null || sha256sum "$out"
done
