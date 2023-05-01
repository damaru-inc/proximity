# proximity
An arduino script that detects proximity.

This is one component of a system that detects when my dog is near the back door, and notifies me by sending a message to a Solace event broker.

There are two components to the system:

1. A proximity sensor connected to a microcontroller, that sends messages to Solace when it detects a presence within 70cm distance.

2. An application that recieves those messages from Solace and makes a dinging sound in response.

This is the first of the two.

The other one is https://github.com/damaru-inc/doorbell


