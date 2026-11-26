#!/usr/bin/env bash
# ==============================================================================
# Project 2: Linux System Health and Resource Monitoring Manager
# Description:
#   Interactive Linux shell application for monitoring system information,
#   CPU, memory, and disk utilization, managing running processes safely,
#   generating timestamped system health reports, and maintaining an activity log.
# ==============================================================================

set -u
export LC_ALL=C

# ==============================================================================
# GLOBAL CONFIGURATION
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/activity.log"
PROTECTED_NAMES_REGEX="^(systemd|init|kthreadd|sshd|dbus-daemon|NetworkManager|login|bash|sh)$"

# ==============================================================================
# UTILITY & VALIDATION FUNCTIONS
# ==============================================================================

# Prints a clean visual divider
print_line() {
  printf '%s\n' "----------------------------------------------------------------------"
}

# Prints a formatted section header
print_header() {
  local title="$1"
  echo ""
  print_line
  printf "  %s\n" "$title"
  print_line
}

# Pauses execution until the user presses Enter
pause_prompt() {
  echo ""
  read -r -p "Press [Enter] to return to the main menu..." _
}

# Checks whether a system command exists
command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# Verifies that core Linux utilities required by the script are installed
check_dependencies() {
  local required_cmds=("ps" "free" "df" "awk" "uname" "uptime" "kill" "date" "hostname")
  local missing=()

  for cmd in "${required_cmds[@]}"; do
    if ! command_exists "$cmd"; then
      missing+=("$cmd")
    fi
  done

  if [ "${#missing[@]}" -gt 0 ]; then
    echo "[ERROR] Missing required system utilities: ${missing[*]}"
    echo "Please install the missing packages before running this application."
    exit 1
  fi
}

# Appends a timestamped entry to the activity log file
log_activity() {
  local level="$1"
  local message="$2"
  local timestamp
  timestamp="$(date '+%Y-%m-%d %H:%M:%S')"

  # Ensure log file directory is writable before appending
  if ! touch "$LOG_FILE" 2>/dev/null; then
    echo "[WARNING] Unable to write to activity log at: $LOG_FILE"
    return 1
  fi

  printf "[%s] [%-7s] %s\n" "$timestamp" "$level" "$message" >> "$LOG_FILE"
}

# Validates that a threshold input is an integer between 1 and 100
validate_threshold() {
  local input="$1"
  if [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -ge 1 ] && [ "$input" -le 100 ]; then
    return 0
  fi
  return 1
}

# Prompts user for explicit confirmation (y/yes vs n/no)
confirm_action() {
  local prompt_msg="$1"
  local reply

  while true; do
    read -r -p "$prompt_msg (y/n): " reply
    case "$reply" in
      [Yy]|[Yy][Ee][Ss]) return 0 ;;
      [Nn]|[Nn][Oo])     return 1 ;;
      *) echo "[ERROR] Invalid response. Please enter 'y' or 'n'." ;;
    esac
  done
}

# ==============================================================================
# METRIC COLLECTION HELPERS
# ==============================================================================

# Calculates current overall CPU utilization percentage using /proc/stat
get_cpu_usage_percent() {
  if [ -r /proc/stat ]; then
    local cpu user1 nice1 sys1 idle1 iowait1 irq1 softirq1 steal1
    local user2 nice2 sys2 idle2 iowait2 irq2 softirq2 steal2

    read -r cpu user1 nice1 sys1 idle1 iowait1 irq1 softirq1 steal1 _ < /proc/stat
    sleep 0.5
    read -r cpu user2 nice2 sys2 idle2 iowait2 irq2 softirq2 steal2 _ < /proc/stat

    local idle_all1=$((idle1 + iowait1))
    local idle_all2=$((idle2 + iowait2))
    local non_idle1=$((user1 + nice1 + sys1 + irq1 + softirq1 + steal1))
    local non_idle2=$((user2 + nice2 + sys2 + irq2 + softirq2 + steal2))

    local total1=$((idle_all1 + non_idle1))
    local total2=$((idle_all2 + non_idle2))

    local total_diff=$((total2 - total1))
    local idle_diff=$((idle_all2 - idle_all1))

    if [ "$total_diff" -gt 0 ]; then
      awk -v td="$total_diff" -v id="$idle_diff" 'BEGIN { printf "%.2f", ((td - id) / td) * 100 }'
      return 0
    fi
  fi

  # Fallback using ps sum across CPU cores if /proc/stat is unavailable
  local cores
  cores="$(nproc 2>/dev/null || echo 1)"
  ps -eo %cpu --no-headers | awk -v c="$cores" '{sum += $1} END {val = sum / c; if (val > 100) val = 100; printf "%.2f", val}'
}

# Retrieves OS distribution name cleanly
get_os_name() {
  if [ -r /etc/os-release ]; then
    awk -F= '/^PRETTY_NAME=/{gsub(/"/, "", $2); print $2}' /etc/os-release
  else
    uname -o
  fi
}

# Retrieves human-readable uptime
get_system_uptime() {
  if uptime -p >/dev/null 2>&1; then
    uptime -p
  else
    uptime | awk -F'(up |, [0-9]+ user)' '{print $2}'
  fi
}

# ==============================================================================
# CORE FEATURE FUNCTIONS (MENU OPTIONS 1 - 9)
# ==============================================================================

# Option 1: Display system information
show_system_info() {
  print_header "1. SYSTEM INFORMATION"

  local host_name os_info kernel_ver sys_uptime current_dt arch_info
  host_name="$(hostname 2>/dev/null || uname -n)"
  os_info="$(get_os_name)"
  kernel_ver="$(uname -r)"
  arch_info="$(uname -m)"
  sys_uptime="$(get_system_uptime)"
  current_dt="$(date '+%Y-%m-%d %H:%M:%S %Z')"

  printf "  %-20s : %s\n" "Hostname" "$host_name"
  printf "  %-20s : %s\n" "Operating System" "$os_info"
  printf "  %-20s : %s (%s)\n" "Kernel Version" "$kernel_ver" "$arch_info"
  printf "  %-20s : %s\n" "System Uptime" "$sys_uptime"
  printf "  %-20s : %s\n" "Current Date & Time" "$current_dt"

  log_activity "INFO" "Viewed system information (Host: $host_name, Kernel: $kernel_ver)."
  pause_prompt
}

# Option 2: Monitor CPU utilization
monitor_cpu() {
  print_header "2. CPU UTILIZATION MONITOR"

  local threshold
  while true; do
    read -r -p "Enter CPU warning threshold percentage (1-100): " threshold
    if validate_threshold "$threshold"; then
      break
    fi
    echo "[ERROR] Invalid threshold. Please enter a whole number between 1 and 100."
  done

  echo ""
  echo "Sampling CPU utilization..."
  local cpu_usage load_avg cpu_cores
  cpu_usage="$(get_cpu_usage_percent)"
  cpu_cores="$(nproc 2>/dev/null || echo 'N/A')"
  load_avg="$(awk '{print $1", "$2", "$3}' /proc/loadavg 2>/dev/null || echo 'N/A')"

  printf "  %-24s : %s%%\n" "Overall CPU Usage" "$cpu_usage"
  printf "  %-24s : %s\n" "Logical CPU Cores" "$cpu_cores"
  printf "  %-24s : %s\n" "Load Average (1,5,15m)" "$load_avg"
  printf "  %-24s : %s%%\n" "Warning Threshold" "$threshold"

  # Compare floating-point CPU usage against integer threshold
  local exceeded
  exceeded="$(awk -v u="$cpu_usage" -v t="$threshold" 'BEGIN { print (u > t) ? 1 : 0 }')"

  echo ""
  if [ "$exceeded" -eq 1 ]; then
    echo "[WARNING] Overall CPU usage (${cpu_usage}%) exceeds threshold (${threshold}%)!"
    log_activity "WARNING" "High CPU usage detected: ${cpu_usage}% (Threshold: ${threshold}%)."
  else
    echo "[OK] Overall CPU usage (${cpu_usage}%) is within safe threshold (${threshold}%)."
    log_activity "INFO" "Monitored CPU usage: ${cpu_usage}% (Threshold: ${threshold}%)."
  fi

  echo ""
  echo "Top 5 CPU-Consuming Processes:"
  print_line
  ps -eo pid,user,%cpu,%mem,comm --sort=-%cpu | head -n 6

  # Flag individual processes exceeding the threshold
  local high_procs
  high_procs="$(ps -eo pid,%cpu,comm --no-headers --sort=-%cpu | awk -v t="$threshold" '$2 > t {printf "PID %s (%s: %s%%); ", $1, $3, $2}')"
  if [ -n "$high_procs" ]; then
    echo ""
    echo "[WARNING] Processes exceeding ${threshold}% CPU: $high_procs"
    log_activity "WARNING" "Processes exceeding ${threshold}% CPU: $high_procs"
  fi

  pause_prompt
}

# Option 3: Monitor memory utilization
monitor_memory() {
  print_header "3. MEMORY UTILIZATION MONITOR"

  local threshold
  while true; do
    read -r -p "Enter memory warning threshold percentage (1-100): " threshold
    if validate_threshold "$threshold"; then
      break
    fi
    echo "[ERROR] Invalid threshold. Please enter a whole number between 1 and 100."
  done

  local mem_line total_mb used_mb free_mb avail_mb mem_pct
  mem_line="$(free -m | awk '/^Mem:/ {print $2, $3, $4, $7}')"
  read -r total_mb used_mb free_mb avail_mb <<< "$mem_line"

  # Fallback if 'available' column is missing on older free versions
  if [ -z "${avail_mb:-}" ]; then
    avail_mb="$free_mb"
  fi

  mem_pct="$(awk -v u="$used_mb" -v t="$total_mb" 'BEGIN { if (t > 0) printf "%.2f", (u / t) * 100; else print "0.00" }')"

  echo ""
  printf "  %-24s : %s MB\n" "Total Memory" "$total_mb"
  printf "  %-24s : %s MB\n" "Used Memory" "$used_mb"
  printf "  %-24s : %s MB\n" "Available Memory" "$avail_mb"
  printf "  %-24s : %s%%\n" "Memory Utilization" "$mem_pct"
  printf "  %-24s : %s%%\n" "Warning Threshold" "$threshold"

  local exceeded
  exceeded="$(awk -v u="$mem_pct" -v t="$threshold" 'BEGIN { print (u > t) ? 1 : 0 }')"

  echo ""
  if [ "$exceeded" -eq 1 ]; then
    echo "[WARNING] Memory utilization (${mem_pct}%) exceeds threshold (${threshold}%)!"
    log_activity "WARNING" "High memory utilization detected: ${mem_pct}% (${used_mb}/${total_mb} MB, Threshold: ${threshold}%)."
  else
    echo "[OK] Memory utilization (${mem_pct}%) is within safe threshold (${threshold}%)."
    log_activity "INFO" "Monitored memory usage: ${mem_pct}% (${used_mb}/${total_mb} MB, Threshold: ${threshold}%)."
  fi

  pause_prompt
}

# Option 4: Monitor disk utilization
monitor_disk() {
  print_header "4. DISK UTILIZATION MONITOR"

  local threshold
  while true; do
    read -r -p "Enter disk warning threshold percentage (1-100): " threshold
    if validate_threshold "$threshold"; then
      break
    fi
    echo "[ERROR] Invalid threshold. Please enter a whole number between 1 and 100."
  done

  echo ""
  echo "Mounted Filesystems (Excluding virtual pseudo-filesystems):"
  print_line
  df -hP -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null || df -hP
  print_line

  local warnings_found=0
  while read -r fs size used avail pct mountpoint; do
    local numeric_pct="${pct%%%}"
    if [[ "$numeric_pct" =~ ^[0-9]+$ ]] && [ "$numeric_pct" -ge "$threshold" ]; then
      echo "[WARNING] Filesystem '$mountpoint' ($fs) is at ${numeric_pct}% capacity (Threshold: ${threshold}%)."
      log_activity "WARNING" "Disk threshold exceeded on $mountpoint ($fs): ${numeric_pct}% used (Threshold: ${threshold}%)."
      warnings_found=$((warnings_found + 1))
    fi
  done < <(df -P -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null | awk 'NR>1 {print $1, $2, $3, $4, $5, $6}')

  if [ "$warnings_found" -eq 0 ]; then
    echo "[OK] All monitored filesystems are below ${threshold}% capacity."
    log_activity "INFO" "Monitored disk utilization (Threshold: ${threshold}%). No warnings triggered."
  fi

  pause_prompt
}

# Option 5: View and search running processes
view_and_search_processes() {
  print_header "5. VIEW & SEARCH RUNNING PROCESSES"

  echo "  1) Display top 20 running processes (sorted by CPU usage)"
  echo "  2) Search for a process by Name or PID"
  echo ""
  read -r -p "Select an option (1 or 2): " sub_choice

  case "$sub_choice" in
    1)
      echo ""
      print_line
      ps -eo pid,user,%cpu,%mem,stat,comm --sort=-%cpu | head -n 21
      print_line
      log_activity "INFO" "Listed top 20 running processes."
      ;;
    2)
      echo ""
      read -r -p "Enter process name or PID to search: " query
      if [ -z "${query// /}" ]; then
        echo "[ERROR] Search query cannot be empty."
        pause_prompt
        return
      fi

      echo ""
      print_line
      printf "%-8s %-14s %-6s %-6s %s\n" "PID" "USER" "%CPU" "%MEM" "COMMAND"
      print_line

      local matches
      if [[ "$query" =~ ^[0-9]+$ ]]; then
        matches="$(ps -p "$query" -o pid=,user=,%cpu=,%mem=,comm= 2>/dev/null)"
      else
        matches="$(ps -eo pid=,user=,%cpu=,%mem=,comm= | awk -v q="$query" 'tolower($5) ~ tolower(q) {printf "%-8s %-14s %-6s %-6s %s\n", $1, $2, $3, $4, $5}')"
      fi

      if [ -z "$matches" ]; then
        echo "No running processes matched: '$query'"
        log_activity "INFO" "Searched processes for '$query' (0 matches found)."
      else
        echo "$matches"
        log_activity "INFO" "Searched processes for '$query' (matches found)."
      fi
      print_line
      ;;
    *)
      echo "[ERROR] Invalid selection. Please enter 1 or 2."
      ;;
  esac

  pause_prompt
}

# Option 6: Terminate a process safely
terminate_process() {
  print_header "6. TERMINATE A PROCESS"

  read -r -p "Enter the PID of the process to terminate: " target_pid

  # Validate numeric PID
  if ! [[ "$target_pid" =~ ^[0-9]+$ ]]; then
    echo "[ERROR] Invalid PID '$target_pid'. A PID must be a positive integer."
    log_activity "ERROR" "Failed termination attempt: Invalid PID '$target_pid'."
    pause_prompt
    return
  fi

  # Verify process exists
  if ! ps -p "$target_pid" >/dev/null 2>&1; then
    echo "[ERROR] No active process found with PID $target_pid."
    log_activity "ERROR" "Failed termination attempt: PID $target_pid does not exist."
    pause_prompt
    return
  fi

  # Retrieve process details
  local proc_user proc_comm proc_cpu proc_mem
  read -r proc_user proc_cpu proc_mem proc_comm < <(ps -p "$target_pid" -o user=,%cpu=,%mem=,comm=)

  echo ""
  echo "Target Process Details:"
  printf "  %-15s : %s\n" "PID" "$target_pid"
  printf "  %-15s : %s\n" "Owner" "$proc_user"
  printf "  %-15s : %s\n" "Command" "$proc_comm"
  printf "  %-15s : %s%% CPU | %s%% MEM\n" "Utilization" "$proc_cpu" "$proc_mem"
  echo ""

  # Block critical system PIDs and current shell/script PIDs
  if [ "$target_pid" -le 2 ] || [ "$target_pid" -eq "$$" ] || [ "$target_pid" -eq "$PPID" ]; then
    echo "[BLOCKED] PID $target_pid ($proc_comm) is a protected system/session process and cannot be terminated."
    log_activity "WARNING" "Blocked termination of protected PID $target_pid ($proc_comm)."
    pause_prompt
    return
  fi

  # Block kernel threads (names wrapped in brackets) and critical system daemons
  if [[ "$proc_comm" =~ ^\[.*\]$ ]] || [[ "$proc_comm" =~ $PROTECTED_NAMES_REGEX ]]; then
    echo "[BLOCKED] '$proc_comm' (PID $target_pid) is a protected system process and cannot be terminated."
    log_activity "WARNING" "Blocked termination of protected system process '$proc_comm' (PID $target_pid)."
    pause_prompt
    return
  fi

  # Check ownership / permissions
  local current_user
  current_user="$(id -un)"
  if [ "$(id -u)" -ne 0 ] && [ "$proc_user" != "$current_user" ]; then
    echo "[ERROR] Permission denied: Process PID $target_pid is owned by '$proc_user', not '$current_user'."
    log_activity "ERROR" "Permission denied attempting to terminate PID $target_pid owned by '$proc_user'."
    pause_prompt
    return
  fi

  # Confirm termination
  if ! confirm_action "Are you sure you want to terminate PID $target_pid ($proc_comm)?"; then
    echo "Operation cancelled. Process $target_pid was not terminated."
    log_activity "INFO" "User cancelled termination of PID $target_pid ($proc_comm)."
    pause_prompt
    return
  fi

  # Send SIGTERM and verify termination
  if kill -15 "$target_pid" 2>/dev/null; then
    sleep 0.5
    if ps -p "$target_pid" >/dev/null 2>&1; then
      echo "[WARNING] Process $target_pid did not exit immediately after SIGTERM."
      if confirm_action "Force terminate PID $target_pid with SIGKILL (-9)?"; then
        if kill -9 "$target_pid" 2>/dev/null; then
          echo "[SUCCESS] Process $target_pid ($proc_comm) was forcefully terminated."
          log_activity "ACTION" "Forcefully terminated PID $target_pid ($proc_comm) with SIGKILL."
        else
          echo "[ERROR] Failed to send SIGKILL to PID $target_pid (insufficient permissions)."
          log_activity "ERROR" "Failed SIGKILL on PID $target_pid ($proc_comm)."
        fi
      else
        log_activity "INFO" "Declined SIGKILL for PID $target_pid ($proc_comm)."
      fi
    else
      echo "[SUCCESS] Process $target_pid ($proc_comm) has been terminated."
      log_activity "ACTION" "Terminated process PID $target_pid ($proc_comm) with SIGTERM."
    fi
  else
    echo "[ERROR] Failed to terminate PID $target_pid. Insufficient permissions or process already exited."
    log_activity "ERROR" "Failed to terminate PID $target_pid ($proc_comm)."
  fi

  pause_prompt
}

# Option 7: Generate a timestamped system health report
generate_health_report() {
  print_header "7. GENERATE SYSTEM HEALTH REPORT"

  echo "Enter the target directory path to save the report"
  read -r -p "(Press [Enter] to use default: $SCRIPT_DIR): " target_dir

  if [ -z "${target_dir// /}" ]; then
    target_dir="$SCRIPT_DIR"
  fi

  # Expand leading tilde (~) if entered by user
  target_dir="${target_dir/#\~/$HOME}"

  # Validate directory existence
  if [ ! -e "$target_dir" ]; then
    echo "[WARNING] Directory '$target_dir' does not exist."
    if confirm_action "Would you like to create this directory?"; then
      if ! mkdir -p "$target_dir" 2>/dev/null; then
        echo "[ERROR] Cannot create directory '$target_dir' (permission denied or invalid path)."
        log_activity "ERROR" "Failed to create report directory '$target_dir'."
        pause_prompt
        return
      fi
      echo "[OK] Created directory: $target_dir"
    else
      echo "Report generation cancelled."
      pause_prompt
      return
    fi
  elif [ ! -d "$target_dir" ]; then
    echo "[ERROR] '$target_dir' exists but is not a directory."
    log_activity "ERROR" "Invalid report path '$target_dir' (not a directory)."
    pause_prompt
    return
  fi

  # Validate write permissions
  if [ ! -w "$target_dir" ]; then
    echo "[ERROR] Directory '$target_dir' is not writable. Please choose an accessible directory."
    log_activity "ERROR" "Report directory '$target_dir' is not writable."
    pause_prompt
    return
  fi

  local file_ts report_ts report_file
  file_ts="$(date '+%Y%m%d_%H%M%S')"
  report_ts="$(date '+%Y-%m-%d %H:%M:%S %Z')"
  report_file="${target_dir%/}/system_health_report_${file_ts}.txt"

  echo "Collecting system health metrics..."
  local cpu_usage
  cpu_usage="$(get_cpu_usage_percent)"

  {
    echo "======================================================================"
    echo "SYSTEM HEALTH REPORT"
    echo "Generated At : $report_ts"
    echo "Hostname     : $(hostname 2>/dev/null || uname -n)"
    echo "OS / Kernel  : $(get_os_name) | $(uname -r)"
    echo "Uptime       : $(get_system_uptime)"
    echo "======================================================================"
    echo ""
    echo "[1] CPU UTILIZATION"
    echo "----------------------------------------------------------------------"
    echo "Overall CPU Usage : ${cpu_usage}%"
    echo "Logical Cores     : $(nproc 2>/dev/null || echo 'N/A')"
    echo "Load Average      : $(awk '{print $1", "$2", "$3}' /proc/loadavg 2>/dev/null || echo 'N/A')"
    echo ""
    echo "Top 5 CPU Processes:"
    ps -eo pid,user,%cpu,%mem,comm --sort=-%cpu | head -n 6
    echo ""
    echo "[2] MEMORY UTILIZATION"
    echo "----------------------------------------------------------------------"
    free -h
    echo ""
    echo "[3] DISK UTILIZATION"
    echo "----------------------------------------------------------------------"
    df -hP -x tmpfs -x devtmpfs -x squashfs -x overlay 2>/dev/null || df -hP
    echo ""
    echo "======================================================================"
    echo "END OF REPORT"
    echo "======================================================================"
  } > "$report_file" 2>/dev/null

  if [ $? -eq 0 ] && [ -f "$report_file" ]; then
    echo "[SUCCESS] System health report saved to:"
    echo "  $report_file"
    log_activity "ACTION" "Generated system health report: $report_file"
  else
    echo "[ERROR] Failed to write report file to '$report_file'."
    log_activity "ERROR" "Failed to write report file '$report_file'."
  fi

  pause_prompt
}

# Option 8: View activity history
view_activity_log() {
  print_header "8. VIEW ACTIVITY LOG"

  if [ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ]; then
    echo "The activity log is currently empty (no recorded events yet)."
  else
    echo "Log File Location: $LOG_FILE"
    print_line
    cat "$LOG_FILE"
    print_line
  fi

  pause_prompt
}

# Option 9: Clear activity log after confirmation
clear_activity_log() {
  print_header "9. CLEAR ACTIVITY LOG"

  if [ ! -f "$LOG_FILE" ] || [ ! -s "$LOG_FILE" ]; then
    echo "The activity log is already empty."
    pause_prompt
    return
  fi

  if confirm_action "Are you sure you want to permanently clear the activity log?"; then
    if : > "$LOG_FILE" 2>/dev/null; then
      echo "[SUCCESS] Activity log has been cleared."
      log_activity "ACTION" "Activity log cleared by user."
    else
      echo "[ERROR] Unable to clear log file '$LOG_FILE' (permission denied)."
    fi
  else
    echo "Operation cancelled. Activity log was preserved."
    log_activity "INFO" "User cancelled clearing the activity log."
  fi

  pause_prompt
}

# ==============================================================================
# INTERACTIVE MAIN MENU LOOP
# ==============================================================================
display_menu() {
  echo ""
  echo "======================================================================"
  echo "        LINUX SYSTEM HEALTH & RESOURCE MONITORING MANAGER"
  echo "======================================================================"
  echo "  1. Display system information"
  echo "  2. Monitor CPU utilization"
  echo "  3. Monitor memory utilization"
  echo "  4. Monitor disk utilization"
  echo "  5. View and search running processes"
  echo "  6. Terminate a process"
  echo "  7. Generate a system health report"
  echo "  8. View activity history"
  echo "  9. Clear activity log"
  echo " 10. Exit the application"
  echo "======================================================================"
}

main() {
  check_dependencies
  log_activity "INFO" "System Health & Resource Monitoring Manager started."

  local choice
  while true; do
    display_menu
    read -r -p "Enter your choice (1-10): " choice

    case "$choice" in
      1)  show_system_info ;;
      2)  monitor_cpu ;;
      3)  monitor_memory ;;
      4)  monitor_disk ;;
      5)  view_and_search_processes ;;
      6)  terminate_process ;;
      7)  generate_health_report ;;
      8)  view_activity_log ;;
      9)  clear_activity_log ;;
      10)
        log_activity "INFO" "System Health & Resource Monitoring Manager exited."
        echo ""
        echo "Exiting System Health Manager. Goodbye!"
        exit 0
        ;;
      *)
        echo "[ERROR] Invalid selection '$choice'. Please enter a number between 1 and 10."
        log_activity "ERROR" "Invalid menu selection entered: '$choice'."
        ;;
    esac
  done
}

main "$@"