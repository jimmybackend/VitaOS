CREATE TABLE boot_session (
    boot_id TEXT PRIMARY KEY,
    node_id TEXT NOT NULL,
    arch TEXT NOT NULL,
    boot_unix INTEGER,
    boot_monotonic_ns INTEGER,
    mode TEXT NOT NULL,
    audit_state TEXT NOT NULL,
    language_mode TEXT NOT NULL DEFAULT 'es,en'
);

CREATE TABLE hardware_snapshot (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    boot_id TEXT NOT NULL,
    cpu_arch TEXT,
    cpu_model TEXT,
    ram_bytes INTEGER,
    firmware_type TEXT,
    console_type TEXT,
    net_count INTEGER,
    storage_count INTEGER,
    usb_count INTEGER,
    wifi_count INTEGER DEFAULT 0,
    detected_at_unix INTEGER NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE audit_event (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    boot_id TEXT NOT NULL,
    event_seq INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    actor_type TEXT NOT NULL,
    actor_id TEXT,
    target_type TEXT,
    target_id TEXT,
    summary TEXT NOT NULL,
    details_json TEXT,
    monotonic_ns INTEGER NOT NULL,
    rtc_unix INTEGER,
    prev_hash TEXT,
    event_hash TEXT NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE ai_proposal (
    proposal_id TEXT PRIMARY KEY,
    boot_id TEXT NOT NULL,
    created_unix INTEGER NOT NULL,
    proposal_type TEXT NOT NULL,
    summary TEXT NOT NULL,
    rationale TEXT NOT NULL,
    risk_level TEXT NOT NULL,
    benefit_level TEXT NOT NULL,
    requires_human_confirmation INTEGER NOT NULL,
    status TEXT NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE human_response (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    proposal_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    response TEXT NOT NULL,
    operator_key TEXT DEFAULT 's/n',
    response_unix INTEGER NOT NULL,
    FOREIGN KEY (proposal_id) REFERENCES ai_proposal(proposal_id),
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE node_peer (
    peer_id TEXT PRIMARY KEY,
    first_seen_unix INTEGER NOT NULL,
    last_seen_unix INTEGER NOT NULL,
    transport TEXT NOT NULL,
    capabilities_json TEXT NOT NULL,
    trust_state TEXT NOT NULL,
    link_state TEXT NOT NULL DEFAULT 'discovered'
);

CREATE TABLE node_task (
    task_id TEXT PRIMARY KEY,
    origin_node_id TEXT NOT NULL,
    target_node_id TEXT NOT NULL,
    task_type TEXT NOT NULL,
    status TEXT NOT NULL,
    created_unix INTEGER NOT NULL,
    updated_unix INTEGER NOT NULL,
    payload_json TEXT
);

CREATE TABLE knowledge_pack (
    pack_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    lang TEXT NOT NULL,
    topic TEXT NOT NULL,
    source_hash TEXT NOT NULL,
    loaded_unix INTEGER NOT NULL
);
