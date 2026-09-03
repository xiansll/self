# P6B Rage Dry-Run Provider Foundation

Added read-only provider contracts for combat frame, weapon, prediction,
candidate entities, bones, hitboxes, lag records, shoot history and rich trace.

Also added ProviderHub, readiness refresh, read-only snapshot publication,
synthetic full-pipeline demo and P6 debug logging.

No live provider is bound by this batch.
No command acquisition or command mutation is added.
Runtime trace/penetration remain BLOCKED until a separately validated trace
provider is supplied.
