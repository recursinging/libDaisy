#include "dev/max11300.h"
#include <gtest/gtest.h>
#include <cmath>
#include <bitset>

using namespace daisy;

#define UNUSED(x) (void)x

class TestTransport
{
  public:
    typedef std::function<bool(uint8_t* buff, size_t size, uint32_t wait_us)>
        TxCallback;
    typedef std::function<bool(uint8_t* tx_buff, uint8_t* rx_buff, size_t size)>
        TxRxCallback;
    /**
     * Transport configuration struct for the MAX11300
     */
    struct Config
    {
        TxCallback   tx_callback;
        TxRxCallback txrx_callback;
        void         Defaults() {}
    };

    enum class Result
    {
        OK, /**< & */
        ERR /**< & */
    };

    void Init(Config config)
    {
        tx_callback   = config.tx_callback;
        txrx_callback = config.txrx_callback;
    }

    bool Ready() { return true; }

    TestTransport::Result Transmit(uint8_t* buff, size_t size, uint32_t wait_us)
    {
        if(!tx_callback(buff, size, wait_us))
        {
            return Result::ERR;
        }
        return Result::OK;
    }

    TestTransport::Result
    TransmitAndReceive(uint8_t* tx_buff, uint8_t* rx_buff, size_t size)
    {
        if(!txrx_callback(tx_buff, rx_buff, size))
        {
            return Result::ERR;
        }
        return Result::OK;
    }

  private:
    TxCallback   tx_callback;
    TxRxCallback txrx_callback;
};


class Max11300TestWrapper
{
  public:
    struct TxTransaction
    {
        std::string          description;
        std::vector<uint8_t> buff;
        size_t               size;
        uint32_t             wait;
    };

    struct TxRxTransaction
    {
        std::string          description;
        std::vector<uint8_t> tx_buff;
        std::vector<uint8_t> rx_buff;
        size_t               size;
    };

    Max11300TestWrapper() {}
    ~Max11300TestWrapper() {}

    void Init()
    {
        MAX11300Driver<TestTransport>::Config max11300_config;
        TestTransport::Config                 transport_config;
        transport_config.tx_callback     = tx_callback;
        transport_config.txrx_callback   = txrx_callback;
        max11300_config.transport_config = transport_config;
        max11300.Init(max11300_config);
    }

    MAX11300Driver<TestTransport> max11300;
    std::vector<TxTransaction>    tx_transactions;
    std::vector<TxRxTransaction>  txrx_transactions;

    size_t tx_transaction_count   = 0;
    size_t txrx_transaction_count = 0;


  private:
    void verifyTxTransaction(uint8_t* buff, size_t size, uint32_t wait_us)
    {
        UNUSED(buff);
        UNUSED(size);
        UNUSED(wait_us);

        tx_transaction_count++;
        if(tx_transactions.size() < tx_transaction_count)
        {
            FAIL() << "Missing TxTransaction fixture";
        }
        TxTransaction t = tx_transactions.at(tx_transaction_count - 1);

        bool tx_valid = true;

        std::string expecting = std::string("[  DETAIL  ] Expecting: ");
        std::string actual    = std::string("[  DETAIL  ] Actual   : ");
        for(std::size_t i = 0; i < size; ++i)
        {
            std::bitset<8> x(t.buff[i]);
            expecting.append(x.to_string());
            expecting.append(" ");

            std::bitset<8> a(buff[i]);
            actual.append(a.to_string());
            actual.append(" ");

            if(buff[i] != t.buff[i])
            {
                tx_valid = false;
            }
        }

        if(!tx_valid)
        {
            std::string message("[  DETAIL  ] Invalid TX: ");
            message.append(t.description);
            message.append("\n");
            message.append(expecting);
            message.append("\n");
            message.append(actual);
            message.append("\n");
            ADD_FAILURE() << message;
        }


    }

    void verifyTxRxTransaction(uint8_t* tx_buff, uint8_t* rx_buff, size_t size)
    {
        UNUSED(tx_buff);
        UNUSED(rx_buff);
        UNUSED(size);

        txrx_transaction_count++;

        if(txrx_transactions.size() < txrx_transaction_count)
        {
            FAIL() << "Missing TxRxTransaction fixture";
        }
        TxRxTransaction t = txrx_transactions.at(txrx_transaction_count - 1);

        bool tx_valid = true;

        std::string expecting = std::string("[  DETAIL  ] Expecting: ");
        std::string actual    = std::string("[  DETAIL  ] Actual   : ");
        for(std::size_t i = 0; i < size; ++i)
        {
            std::bitset<8> x(t.tx_buff[i]);
            expecting.append(x.to_string());
            expecting.append(" ");

            std::bitset<8> a(tx_buff[i]);
            actual.append(a.to_string());
            actual.append(" ");

            if(tx_buff[i] != t.tx_buff[i])
            {
                tx_valid = false;
            }
        }

        if(!tx_valid)
        {
            std::string message("[  DETAIL  ] Invalid TX: ");
            message.append(t.description);
            message.append("\n");
            message.append(expecting);
            message.append("\n");
            message.append(actual);
            message.append("\n");
            ADD_FAILURE() << message;
        }

        for(std::size_t i = 0; i < t.rx_buff.size(); ++i)
        {
            rx_buff[i] = t.rx_buff[i];
        }
    }


    TestTransport::TxCallback tx_callback
        = [this](uint8_t* buff, size_t size, uint32_t wait_us) -> bool {
        verifyTxTransaction(buff, size, wait_us);
        return true;
    };

    TestTransport::TxRxCallback txrx_callback
        = [this](uint8_t* tx_buff, uint8_t* rx_buff, size_t size) -> bool {
        verifyTxRxTransaction(tx_buff, rx_buff, size);
        return true;
    };
};


TEST(dev_MAX11300, spiCommsInit)
{
    Max11300TestWrapper wrapper;

    Max11300TestWrapper::TxRxTransaction txrx_device_id;
    txrx_device_id.description = "Initial device ID verification";
    txrx_device_id.tx_buff = {(MAX11300_DEVICE_ID << 1) | 1, 0x00, 0x00};
    txrx_device_id.rx_buff = {0x00, (uint8_t)(0x0424 >> 8), (uint8_t)0x0424};
    txrx_device_id.size    = 3;
    wrapper.txrx_transactions.emplace_back(txrx_device_id);

    Max11300TestWrapper::TxTransaction tx_devicectl;
    tx_devicectl.description = "Initial device configuration";
    tx_devicectl.buff = {(MAX11300_DEVCTL << 1), 0x41, 0xF7};
    tx_devicectl.size = 3;
    tx_devicectl.wait = 0;
    wrapper.tx_transactions.emplace_back(tx_devicectl);

    Max11300TestWrapper::TxRxTransaction txrx_devicectl_verify;
    txrx_devicectl_verify.description = "Initial device configuration verification";
    txrx_devicectl_verify.tx_buff = {(MAX11300_DEVCTL << 1) | 1, 0x00, 0x00};
    txrx_devicectl_verify.rx_buff = {0x00, 0x41, 0xF7};
    txrx_devicectl_verify.size    = 3;
    wrapper.txrx_transactions.emplace_back(txrx_devicectl_verify);


    wrapper.Init();
}


TEST(dev_MAX11300, a_VoltsTo12BitUint)
{
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(-1, MAX11300::VoltageRange::ZERO_TO_10), 0);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(0, MAX11300::VoltageRange::ZERO_TO_10),
              0);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(2.5, MAX11300::VoltageRange::ZERO_TO_10),
        1023);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(5, MAX11300::VoltageRange::ZERO_TO_10),
              2047);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(7.5, MAX11300::VoltageRange::ZERO_TO_10),
        3071);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(10, MAX11300::VoltageRange::ZERO_TO_10),
        4095);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(12, MAX11300::VoltageRange::ZERO_TO_10),
        4095);

    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -5.5, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
              0);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(-5, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
        0);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -2.5, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
              1023);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(0, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
        2047);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  2.5, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
              3071);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(5, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
        4095);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(7, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
        4095);

    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -12, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
              0);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -10, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
              0);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -7.5, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
              1023);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -5, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
              2047);
    EXPECT_EQ(MAX11300::voltsTo12BitUint(
                  -2.5, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
              3071);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(0, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
        4095);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(2, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
        4095);

    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(-1, MAX11300::VoltageRange::ZERO_TO_2_5), 0);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(0, MAX11300::VoltageRange::ZERO_TO_2_5), 0);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(1, MAX11300::VoltageRange::ZERO_TO_2_5),
        1638);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(1.25, MAX11300::VoltageRange::ZERO_TO_2_5),
        2047);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(2, MAX11300::VoltageRange::ZERO_TO_2_5),
        3276);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(2.5, MAX11300::VoltageRange::ZERO_TO_2_5),
        4095);
    EXPECT_EQ(
        MAX11300::voltsTo12BitUint(5, MAX11300::VoltageRange::ZERO_TO_2_5),
        4095);
}

TEST(dev_MAX11300, b_TwelveBitUintToVolts)
{
    float oneLsbAtTenVolts          = 10.0f / 4096.0f;
    float oneLsbAtTwoPointFiveVolts = 2.5f / 4096.0f;

    EXPECT_FLOAT_EQ(
        MAX11300::TwelveBitUintToVolts(0, MAX11300::VoltageRange::ZERO_TO_10),
        0);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    1023, MAX11300::VoltageRange::ZERO_TO_10),
                2.5,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    2047, MAX11300::VoltageRange::ZERO_TO_10),
                5,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    3071, MAX11300::VoltageRange::ZERO_TO_10),
                7.5,
                oneLsbAtTenVolts);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        4095, MAX11300::VoltageRange::ZERO_TO_10),
                    10);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        5000, MAX11300::VoltageRange::ZERO_TO_10),
                    10);

    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        0, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                    -5);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    1023, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                -2.5,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    2047, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                0,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    3071, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                2.5,
                oneLsbAtTenVolts);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        4095, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                    5);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        5000, MAX11300::VoltageRange::NEGATIVE_5_TO_5),
                    5);

    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        0, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                    -10);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    1023, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                -7.5,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    2047, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                -5,
                oneLsbAtTenVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    3071, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                -2.5,
                oneLsbAtTenVolts);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        4095, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                    0);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        5000, MAX11300::VoltageRange::NEGATIVE_10_TO_0),
                    0);

    EXPECT_FLOAT_EQ(
        MAX11300::TwelveBitUintToVolts(0, MAX11300::VoltageRange::ZERO_TO_2_5),
        0);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    1638, MAX11300::VoltageRange::ZERO_TO_2_5),
                1,
                oneLsbAtTwoPointFiveVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    2047, MAX11300::VoltageRange::ZERO_TO_2_5),
                1.25,
                oneLsbAtTwoPointFiveVolts);
    EXPECT_NEAR(MAX11300::TwelveBitUintToVolts(
                    3276, MAX11300::VoltageRange::ZERO_TO_2_5),
                2,
                oneLsbAtTwoPointFiveVolts);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        4095, MAX11300::VoltageRange::ZERO_TO_2_5),
                    2.5);
    EXPECT_FLOAT_EQ(MAX11300::TwelveBitUintToVolts(
                        5000, MAX11300::VoltageRange::ZERO_TO_2_5),
                    2.5);
}
