// Below infomation you can set after signing up with u-blox Thingstream portal 
// and after add a new New PointPerfect Thing
// https://portal.thingstream.io/app/location-services/things
// in the new PointPerfect Thing you go to the credentials page and copy paste the values and certificate into this.  

// <Your PointPerfect Thing> -> Credentials -> Client Id
static const char MQTT_CLIENT_ID[] = "<ADD YOUR CLIENT ID HERE>";

// <Your PointPerfect Thing> -> Credentials -> Amazon Root Certificate
static const char AWS_CERT_CA[] PROGMEM = R"(-----BEGIN CERTIFICATE-----
<ADD THE ROOT CERTIFICATE HERE>
-----END CERTIFICATE-----)";

// <Your PointPerfect Thing> -> Credentials -> Client Certificate
static const char AWS_CERT_CRT[] PROGMEM = R"(-----BEGIN CERTIFICATE-----
<ADD YOUR CLIENT CERTIFICATE HERE>
-----END CERTIFICATE-----)";

// <Your PointPerfect Thing> -> Credentials -> Client Key
static const char AWS_CERT_PRIVATE[] PROGMEM = R"(-----BEGIN RSA PRIVATE KEY-----
<ADD YOUR CLIENT KEY HERE>
-----END RSA PRIVATE KEY-----)";
