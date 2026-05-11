# About

This repository is the API for the [uFilterPickerPickBroker](https://github.com/uofuseismo/uFilterPickBroker) - a utility that collects and deduplicates pick messages published by various instances running the [uFilterPicker](https://github.com/uofuseismo/uFilterPicker) software.  Those picks are then streams to consumers in the UUSS Kubernetes system.  This a pub/sub pattern backed by a message store .  The message store allows for backfill of picks up to a fixed duration (e.g., 15 minutes). 
