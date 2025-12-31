CREATE TABLE LINEITEM (
    L_ORDERKEY       NUMBER(11)            NOT NULL,
    L_PARTKEY        NUMBER(11)            NOT NULL,
    L_SUPPKEY        NUMBER(11)            NOT NULL,
    L_LINENUMBER     NUMBER(2)             NOT NULL,
    L_QUANTITY       NUMBER(15,2)          NOT NULL,
    L_EXTENDEDPRICE  NUMBER(15,2)          NOT NULL,
    L_DISCOUNT       NUMBER(15,2)          NOT NULL,
    L_TAX            NUMBER(15,2)          NOT NULL,
    L_RETURNFLAG     CHAR(1)               NOT NULL,
    L_LINESTATUS     CHAR(1)               NOT NULL,
    L_SHIPDATE       DATE                  NOT NULL,
    L_COMMITDATE     DATE                  NOT NULL,
    L_RECEIPTDATE    DATE                  NOT NULL,
    L_SHIPINSTRUCT   CHAR(25)              NOT NULL,
    L_SHIPMODE       CHAR(10)              NOT NULL,
    L_COMMENT        VARCHAR2(44)          NOT NULL
)