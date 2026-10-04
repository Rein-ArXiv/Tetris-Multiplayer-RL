# Shared validation for the native Linux x64 release entry points.
# Sourced functions do not alter caller options or create output directories.
release_bool() {
  local _rb_name=${1-} _rb_value=${2-}
  case "${_rb_value,,}" in
    1|on|true|yes|y)
      printf '1'
      return 0
      ;;
    0|off|false|no|n)
      printf '0'
      return 0
      ;;
    *)
      printf '%s\n' "release_bool: ${_rb_name}: invalid boolean value: '${_rb_value}'" >&2
      return 2
      ;;
  esac
}

require_linux_x64() {
  local _rlx_sys _rlx_mach
  _rlx_sys=$(uname -s) || return 2
  _rlx_mach=$(uname -m) || return 2

  if [ "${_rlx_sys}" != "Linux" ]; then
    printf '%s\n' "require_linux_x64: this bundle supports native Linux x64 only (uname -s: ${_rlx_sys})" >&2
    return 2
  fi

  case "${_rlx_mach}" in
    x86_64|amd64)
      return 0
      ;;
    *)
      printf '%s\n' "require_linux_x64: this bundle supports native Linux x64 only (uname -m: ${_rlx_mach})" >&2
      return 2
      ;;
  esac
}