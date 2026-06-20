CREATE TABLE IF NOT EXISTS doc.defects (
	work_order_id int4,
	manufacturing_order_id varchar,
	workcenter_id int4,
	product_id int4,
	product_name varchar,
	defect_code varchar,
	defect_reason_id int4,
	defect_quantity int4,
	ts timestamp
);

CREATE TABLE IF NOT EXISTS doc.factory_events (
	"time" timestamp,
	workcenter varchar,
	event_type varchar,
	workorder_id varchar,
	manufacturing_order_id varchar,
	pause_reason varchar
);

CREATE TABLE IF NOT EXISTS doc.oee_metrics (
	ts timestamp,
	workcenter varchar,
	availability float8,
	performance float8,
	quality float8,
	oee float8
);

CREATE TABLE IF NOT EXISTS doc.robot_events (
	event_type varchar,
	reason varchar,
	status_code int4,
	roda1 int4,
	roda2 int4,
	obstaculo bool,
	workstation varchar,
	workorder_id varchar,
	ts timestamptz
);
