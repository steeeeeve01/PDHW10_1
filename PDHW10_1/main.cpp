//
//  main.cpp
//  PDHW10_1
//
//  Created by 簡紹益 on 2025/11/24.
//

#include <cstring>
#include <iostream>
using namespace std;

// MARK: #1 Global_Constant_Declaration
const int PLATE_LENGTH = 7;
const int BRAND_MAX_LENGTH = 20;
const int BASIC_LEVEL_LIMIT = 3;
const int BASICSTATION_LEVEL = 1;
const int ADVANCEDSTATION_LEVEL = 2;
const int ADVANCEDSTATION_SERVICE_CHARGE = 500;

// MARK: #2 SelfDefined_Object(Car & Order)
struct Car {
    char plate[PLATE_LENGTH + 1]; //+1 for '\0'
    char brand[BRAND_MAX_LENGTH + 1];
    int level;
    int dailyRent;
    int mileage;
    int stationid;
};

struct Order {
    int order_id;
    int originStation_id;
    int destStation_id;
    int level;
    int period;
    int daytoaddback;
    Car *selected_car;
};
// MARK: #3 SelfDefined_Class(Parent: Station; Child: Basic and Advanced
// Station)
class Station {
  protected:
    int station_id;    // 1-based
    int station_level; // 1:Basic, 2:Advanced
    int total_cars;
    int total_cars_limit;
    Car **carAddrs;

  public:
    // constructor & destructor
    // revised_version compared with HW8: 用冒號在前面初始化變數
    Station(int id, int K, int station_level)
        : station_id(id), station_level(station_level), total_cars(0),
          total_cars_limit(K), carAddrs(new Car *[K]) {}
    ~Station() { delete[] carAddrs; }

    // member function
    void addCar(Car *aCarAddr) {
        carAddrs[total_cars] = aCarAddr;
        total_cars += 1;
        aCarAddr->stationid = station_id;
        return;
    }
    void removeCar(Car *rCarAddr) {
        for (int i = 0; i < total_cars; i++) {
            if (carAddrs[i] == rCarAddr) {
                removeCar(i);
                return;
            }
        }
    }
    void removeCar(int id_in_station) {
        total_cars -= 1;
        carAddrs[id_in_station] = carAddrs[total_cars];
        carAddrs[total_cars] = nullptr;
        return;
    }
    int get_Station_Level() { return station_level; }
    virtual bool get_Station_Status() {
        if (total_cars < total_cars_limit)
            return true;
        else
            return false;
    }

    // operator overloading
    const Station &operator+=(Car *carPtr) {
        addCar(carPtr);
        return *this;
    }
    Car *operator-=(const char plate[]);
    Car *operator-=(Car *carPtr) {
        Car *rCarAddr = nullptr;
        for (int i = 0; i < total_cars; i++) {
            if (carAddrs[i] == carPtr) {
                rCarAddr = carAddrs[i];
                removeCar(i);
                return rCarAddr;
            }
        }
        return nullptr;
    }
    friend int count_revenue_algorithm(Order *orders, int N, int M,
                                       Station **stations);
};

class BasicStation : public Station {
  private:
  public:
    BasicStation(int id, int T) : Station(id, T, BASICSTATION_LEVEL) {}
    // bool get_Station_Status() 直接繼承母類別函式
};

class AdvancedStation : public Station {
  private:
  public:
    AdvancedStation(int id, int T) : Station(id, T, ADVANCEDSTATION_LEVEL) {}
    bool get_Station_Status() { return true; }
};

// MARK: #4 Self_Defined_Class: CarsToAddBack
class CarsToAddBack {
  private:
    Order **list;
    int left_idx;
    int right_idx;

  public:
    CarsToAddBack(int capacity) {
        // 使用滑動的左右指針，管理目前用到的地方
        // 新車輸入 -> 右界右移
        // 舊車返回 -> 左界右移
        // 左指針=右指針 -> 只有一台車
        // 左指針>右指針 -> 半台都沒有了
        list = new Order *[capacity * 2]; // solve：為何需要開到*2？
        left_idx = 0;
        right_idx = 0;
    }
    ~CarsToAddBack() { delete[] list; }
    void add_to_list(Order *order) {
        order->daytoaddback = order->order_id + (order->period) - 1;
        // 訂單內有複數訂單，使用insertion_sort
        int i;
        for (i = right_idx - 1; i >= left_idx; i--) {
            if (list[i]->daytoaddback > order->daytoaddback ||
                (list[i]->daytoaddback == order->daytoaddback &&
                 list[i]->order_id > order->order_id)) {
                list[i + 1] = list[i];
            } else {
                break;
            }
        }
        list[i + 1] = order;
        right_idx += 1;
    }
    void remove_back_to_station(int current_day, Station **stations) {
        while (left_idx < right_idx) {
            if (list[left_idx]->daytoaddback == current_day) {
                Station *dest_station =
                    stations[list[left_idx]->destStation_id];
                Car *returning_car = list[left_idx]->selected_car;
                bool is_advanced_dest = (dest_station->get_Station_Level() ==
                                         ADVANCEDSTATION_LEVEL);
                bool is_low_level_car =
                    (returning_car->level <= BASIC_LEVEL_LIMIT);
                bool has_space = dest_station->get_Station_Status();
                if (is_advanced_dest || (is_low_level_car && has_space)) {
                    dest_station->addCar(returning_car);
                    // test
                    // cout<<"return selected car "<<returning_car->plate<<"
                    // back to station "<<list[left_idx]->destStation_id<<endl;
                } else {
                    // 原站無法停（儲位不夠/等級不夠）->存到Station1
                    stations[1]->addCar(returning_car);
                    // test
                    // cout<<"return selected car "<<returning_car->plate<<"
                    // back to station 1"<<endl;
                }
                left_idx += 1;
            } else
                return;
        }
    }
};
// MARK: #5 Algorithm: Count the revenue
int count_revenue_algorithm(Order *orders, int N, int M, Station **stations) {
    int total_revenue = 0;
    CarsToAddBack cars_to_add_back(M);
    for (int i = 1; i <= M; i++) {
        Order &order = orders[i];
        Station *origin_station = stations[order.originStation_id];
        Car *selected_car = nullptr;
        Car *selected_car_otherstation = nullptr;
        Station *selected_station = nullptr;
        // 條件檢查，想在普通站叫高級車就直接作廢
        if (origin_station->get_Station_Level() == BASICSTATION_LEVEL &&
            order.level > BASIC_LEVEL_LIMIT) {
            continue;
        }
        // Method1: 能在原站拿，就在原站拿
        for (int j = 0; j < origin_station->total_cars; j++) {
            Car *car = origin_station->carAddrs[j];
            if (car->level >= order.level) {
                if (selected_car == nullptr ||
                    car->level < selected_car->level ||
                    (car->level == selected_car->level &&
                     car->dailyRent > selected_car->dailyRent) ||
                    (car->level == selected_car->level &&
                     car->dailyRent == selected_car->dailyRent &&
                     strcmp(car->plate, selected_car->plate) < 0)) {
                    selected_station = origin_station;
                    selected_car = car;
                }
            }
        }
        // Method2: 不能在原站拿，那就對每一個站進行訪問，可以的話就進行夜間調度
        if (selected_car == nullptr) {
            for (int j = 1; j <= N; j++) {
                Station *curStation = stations[j];
                if (curStation->get_Station_Level() == BASICSTATION_LEVEL &&
                    order.level > BASIC_LEVEL_LIMIT)
                    continue;
                for (int k = 0; k < curStation->total_cars; k++) {
                    Car *car = curStation->carAddrs[k];
                    if (car->level >= order.level) {
                        if (selected_car_otherstation == nullptr ||
                            car->level < selected_car_otherstation->level ||
                            (car->level == selected_car_otherstation->level &&
                             car->dailyRent >
                                 selected_car_otherstation->dailyRent) ||
                            (car->level == selected_car_otherstation->level &&
                             car->dailyRent ==
                                 selected_car_otherstation->dailyRent &&
                             strcmp(car->plate,
                                    selected_car_otherstation->plate) < 0)) {
                            selected_station = curStation;
                            selected_car_otherstation = car;
                        }
                    }
                }
            }
        }
        order.selected_car = selected_car;
        if (selected_car != nullptr) {
            if (selected_station->get_Station_Level() ==
                ADVANCEDSTATION_LEVEL) {
                // cout<<"(with charge)revenue+"<<selected_car->dailyRent *
                // order.period + ADVANCEDSTATION_SERVICE_CHARGE *
                // selected_car->level<<endl;
                total_revenue +=
                    selected_car->dailyRent * order.period +
                    ADVANCEDSTATION_SERVICE_CHARGE * selected_car->level;
            } else {
                // test
                // cout<<"(without charge)revenue+"<<selected_car->dailyRent *
                // order.period<<endl;
                total_revenue += selected_car->dailyRent * order.period;
            }
            // test
            // cout<<"removed selected car "<<selected_car->plate<<" from
            // origin_station "<<order.originStation_id<<endl;

            origin_station->removeCar(selected_car);
            cars_to_add_back.add_to_list(&order);
        } else if (selected_car_otherstation != nullptr) {
            order.selected_car = selected_car_otherstation;
            Car *car = selected_car_otherstation;
            if (stations[order.originStation_id]->get_Station_Level() ==
                ADVANCEDSTATION_LEVEL) {
                // test
                // cout<<"(with
                // charge)revenue+"<<selected_car_otherstation->dailyRent *
                // order.period + ADVANCEDSTATION_SERVICE_CHARGE *
                // selected_car_otherstation->level<<endl;
                total_revenue +=
                    selected_car_otherstation->dailyRent * order.period +
                    ADVANCEDSTATION_SERVICE_CHARGE *
                        selected_car_otherstation->level;
            } else {
                // cout<<"(without
                // charge)revenue+"<<selected_car_otherstation->dailyRent *
                // order.period<<endl;
                total_revenue +=
                    selected_car_otherstation->dailyRent * order.period;
            }
            // test
            // cout<<"removed selected car from support_station
            // "<<order.originStation_id<<endl;
            Station *support_station = selected_station;
            support_station->removeCar(car);
            cars_to_add_back.add_to_list(&order);
        }
        cars_to_add_back.remove_back_to_station(i, stations);
    }
    return total_revenue;
}
int main() {
    // Info Input
    int N = 0, T = 0;
    int K = 0, NumofAdvancedStation = 0;
    cin >> N >> T;
    cin >> K >> NumofAdvancedStation;
    Station **stations = new Station *[N + 1];
    for (int i = 1; i <= N; i++)
        stations[i] = nullptr;
    int AdvancedStationId = 0;
    for (int i = 0; i < NumofAdvancedStation; i++) {
        cin >> AdvancedStationId;
        stations[AdvancedStationId] = new AdvancedStation(AdvancedStationId, T);
    }
    for (int i = 1; i <= N; i++)
        if (stations[i] == nullptr)
            stations[i] = new BasicStation(i, K);

    Car *cars = new Car[T + 1];
    for (int i = 1; i <= T; i++) {
        cin >> cars[i].plate >> cars[i].brand >> cars[i].level >>
            cars[i].dailyRent >> cars[i].mileage >> cars[i].stationid;
        stations[cars[i].stationid]->addCar(&cars[i]);
    }

    int M;
    cin >> M;
    Order *orders = new Order[M + 1];
    for (int i = 1; i <= M; i++) {
        cin >> orders[i].originStation_id;
        cin >> orders[i].destStation_id;
        cin >> orders[i].level;
        cin >> orders[i].period;
        orders[i].daytoaddback = 0;
        orders[i].order_id = i;
        orders[i].selected_car = nullptr;
    }
    int revenue = count_revenue_algorithm(orders, N, M, stations);
    // Output
    cout << revenue;

    // Release the Dynamic Memory Allocated
    for (int i = 1; i <= N; i++) {
        delete stations[i];
    }
    delete[] stations;
    delete[] cars;
    delete[] orders;
    return 0;
}
