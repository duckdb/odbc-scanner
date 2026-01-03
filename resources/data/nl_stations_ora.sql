CREATE TABLE "nl_train_stations" (
  "id"          NUMBER(19),
  "code"        VARCHAR2(255),
  "uic"         NUMBER(19),
  "name_short"  VARCHAR2(255),
  "name_medium" VARCHAR2(255),
  "name_long"   VARCHAR2(255),
  "slug"        VARCHAR2(255),
  "country"     VARCHAR2(255),
  "type"        VARCHAR2(255),
  "geo_lat"     NUMBER(18,8),
  "geo_lng"     NUMBER(18,8)  
)