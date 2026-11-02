#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

using json = nlohmann::json;
using namespace std;

// ---------- Abstraction + Inheritance + Polymorphism ----------
class Vehicle {
protected:
    int id;
    string brand, model;
    double ratePerDay;
    bool available;
public:
    Vehicle(int i, string b, string m, double r)
        : id(i), brand(b), model(m), ratePerDay(r), available(true) {}
    virtual ~Vehicle() {}

    virtual double calculateRent(int days) const = 0;   // pure virtual
    virtual string type() const = 0;

    int getId() const { return id; }
    bool isAvailable() const { return available; }
    void setAvailable(bool a) { available = a; }

    json toJson() const {
        return { {"id", id}, {"type", type()}, {"brand", brand},
                 {"model", model}, {"ratePerDay", ratePerDay},
                 {"available", available} };
    }
};

class Car : public Vehicle {
public:
    using Vehicle::Vehicle;
    double calculateRent(int days) const override {
        return ratePerDay * days + 500;                 // flat insurance
    }
    string type() const override { return "Car"; }
};

class Bike : public Vehicle {
public:
    using Vehicle::Vehicle;
    double calculateRent(int days) const override {
        return ratePerDay * days;
    }
    string type() const override { return "Bike"; }
};

// ---------- Booking ----------
struct Booking {
    int id, vehicleId, days;
    string customerName;
    double amount;
    bool active;
};

// ---------- Manager (DSA: hash maps) ----------
class RentalManager {
    unordered_map<int, shared_ptr<Vehicle>> vehicles;
    unordered_map<int, Booking> bookings;
    int nextVehicleId = 1, nextBookingId = 1;
public:
    int addVehicle(const string& type, const string& brand,
                   const string& model, double rate) {
        int id = nextVehicleId++;
        if (type == "Car")       vehicles[id] = make_shared<Car>(id, brand, model, rate);
        else if (type == "Bike") vehicles[id] = make_shared<Bike>(id, brand, model, rate);
        else throw runtime_error("Unknown vehicle type");
        return id;
    }

    json listVehicles() const {
        json arr = json::array();
        for (auto& [id, v] : vehicles) arr.push_back(v->toJson());
        return arr;
    }

    json book(int vehicleId, const string& customer, int days) {
        auto it = vehicles.find(vehicleId);
        if (it == vehicles.end()) throw runtime_error("Vehicle not found");
        if (!it->second->isAvailable()) throw runtime_error("Vehicle already booked");
        it->second->setAvailable(false);

        Booking b{nextBookingId++, vehicleId, days, customer,
                  it->second->calculateRent(days), true};
        bookings[b.id] = b;
        return { {"bookingId", b.id}, {"amount", b.amount} };
    }

    json returnVehicle(int bookingId, int extraDays) {
        auto it = bookings.find(bookingId);
        if (it == bookings.end() || !it->second.active)
            throw runtime_error("Invalid booking");
        Booking& b = it->second;
        b.active = false;
        vehicles[b.vehicleId]->setAvailable(true);
        double lateFee = extraDays * 300.0;             // example late fee
        return { {"totalPaid", b.amount + lateFee}, {"lateFee", lateFee} };
    }
};

int main() {
    RentalManager mgr;
    mgr.addVehicle("Car", "Maruti", "Swift", 1500);     // sample data
    mgr.addVehicle("Bike", "Honda", "Activa", 400);

    httplib::Server svr;
    svr.set_mount_point("/", "../frontend");            // serves your website

    svr.Get("/api/vehicles", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(mgr.listVehicles().dump(), "application/json");
    });

    svr.Post("/api/vehicles", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            int id = mgr.addVehicle(j["type"], j["brand"], j["model"], j["ratePerDay"]);
            res.set_content(json{{"id", id}}.dump(), "application/json");
        } catch (exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/book", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            res.set_content(mgr.book(j["vehicleId"], j["customer"], j["days"]).dump(),
                            "application/json");
        } catch (exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/return", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            res.set_content(mgr.returnVehicle(j["bookingId"], j["extraDays"]).dump(),
                            "application/json");
        } catch (exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    cout << "Server running at http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
}