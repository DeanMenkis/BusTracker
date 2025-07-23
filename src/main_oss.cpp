// Translink Bus Tracker using ESP32 and GTFS-Realtime
// This code fetches real-time bus data from Translink's GTFS API and decodes it using Nanopb.

// AUTHOR: DEAN DAVID MENKIS
// DATE: 2025-07-22
#include <WiFi.h>
#include <HTTPClient.h>

// Nanopb core
#include "pb_decode.h"
#include "pb_common.h"
#include "gtfs-realtime.pb.h"

// WiFi and API configuration
const char *ssid = "PUT YOURS HERE";
const char *password = "PUT PASSWORD HERE";
const char *apiKey = "PUT YOUR GTFS API KEY HERE";
const char *apiUrl = "PUT YOUR API URL HERE";
const long fetchInterval = 15000; // 15 seconds

const char *target_route_id = "PUT ROUTE ID HERE (FIND FROM TRANSIT AUTHORITY)"; 

// read data from wifi client stream
bool wifi_client_stream_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
  WiFiClient *client = (WiFiClient *)stream->state;
  if (client == nullptr || !client->connected())
  {
    return false;
  }
  if (client->readBytes(buf, count) == count)
  {
    return true;
  }
  else
  {
    stream->bytes_left = 0;
    return false;
  }
} // end wifi_client_stream_callback

// Decode
bool decode_entity_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  int *vehicle_count = (int *)*arg;

  transit_realtime_FeedEntity entity = transit_realtime_FeedEntity_init_zero;

  if (!pb_decode(stream, transit_realtime_FeedEntity_fields, &entity))
  {
    Serial.printf("Decoding sub-message failed: %s\n", PB_GET_ERROR(stream));
    return false;
  }
  // Check if entity is a vehicle position update
  // do i want to use latitude longitude or stop_id
  if (entity.has_vehicle && strcmp(entity.vehicle.trip.route_id, target_route_id) == 0)
  {
        (*vehicle_count)++;
        // Print bus detail
        Serial.printf("  > Found Route 25 Bus! Vehicle ID: %s, Lat: %f, Lon: %f\n",
            entity.vehicle.vehicle.id,
            entity.vehicle.position.latitude,
            entity.vehicle.position.longitude
        );
  }

  return true;
} // end  decode_entity_callback

void setup()
{
  Serial.begin(115200);
  Serial.println("\nStarting Translink Bus Tracker...");

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  } // loop until connected
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
} // end setup

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String fullUrl = String(apiUrl) + String(apiKey);
    Serial.print("Making API request to: ");
    Serial.println(fullUrl);

    http.begin(fullUrl);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      Serial.printf("HTTP Response code: %d\n", httpCode);

      WiFiClient *clientStream = http.getStreamPtr();

      pb_istream_t pb_stream = {&wifi_client_stream_callback, clientStream, (size_t)-1, 0};

      // passback to callback then catch the quarterback
      int route25_count = 0;

      transit_realtime_FeedMessage message = transit_realtime_FeedMessage_init_zero;

      // adress passed as counter
      message.entity.funcs.decode = &decode_entity_callback;
      message.entity.arg = &route25_count;

      bool status = pb_decode(&pb_stream, transit_realtime_FeedMessage_fields, &message);

      if (status)
      {
        Serial.printf("Found %d bus(es) on Route 25 this cycle.\n", route25_count);
      }
      else
      {
        Serial.println("Protobuf decoding failed");
        Serial.printf("Nanopb error: %s\n", PB_GET_ERROR(&pb_stream));
      }
    }
    else
    {
      Serial.printf("HTTP request failed: %d\n", httpCode);
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected.");
  }

  Serial.printf("\nWaiting %ld seconds for next fetch...\n\n", fetchInterval / 1000);
  delay(fetchInterval);
} // end loop