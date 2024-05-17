'use strict';

const async = require('async')
const request = require('request')
const cassandra = require('cassandra-driver')
const { email, password } = require('./credentials')

let token
let tokenExpires = 0

const client = new cassandra.Client({
  contactPoints: ['10.0.0.80'],
  localDataCenter: 'datacenter1',
  keyspace: 'cgm'
})

const options = {
  headers: {
    product: 'llu.android',
    version: '4.9.0',
    'user-agent': 'curl/8.4.0',
    'accept': '*/*'
  },
  json: true
}

function run() {
  async.waterfall(
    [
      (callback) => {
        if (new Date().valueOf() > tokenExpires) {
          console.log("Logging in...")
          const loginOptions = JSON.parse(JSON.stringify(options))
          loginOptions['body'] = {
            email,
            password
          }
          loginOptions['email'] = email
          loginOptions['passwprd'] = password
          return request.post('https://api.libreview.io/llu/auth/login', loginOptions, (err, response, body) => {
            if (err) {
              return callback(err)
            }

            if (!response ||
                (response.statusCode != 200)) {
              return callback(new Error(`Bad login HTTP response: ${response && response.statusCode}`))
            }

            token = body.data.authTicket.token
            tokenExpires = (body.data.authTicket.expires * 1000) - 600000 // Ten minutes
            console.log(`New token expires at ${new Date(tokenExpires)}`)

            return callback(null)
          })
        }

        return callback(null)
      },
      (callback) => {
        const connectionOptions = JSON.parse(JSON.stringify(options))
        connectionOptions['headers']['Authorization'] = `Bearer ${token}`
        request.get('https://api.libreview.io/llu/connections', connectionOptions, (err, response, body) => {
          if (err) {
            return callback(err)
          }

          if (!response ||
              (response.statusCode != 200)) {
            return callback(new Error(`Bad connections HTTP response: ${response && response.statusCode}`))
          }

          const data = body.data[0]
          const patientId = data.patientId
          const serialNumber = data.sensor.sn
          const mgPerDl = data.glucoseMeasurement.ValueInMgPerDl
          const timestamp = new Date(data.glucoseMeasurement.Timestamp)
          const trendArrow = data.glucoseMeasurement.TrendArrow
          console.log(`Reading of ${mgPerDl} mg/dL (${trendArrow}) taken at ${new Date(timestamp).toISOString()}`)

          const query = 'INSERT INTO cgm.readings (patient_id, timestamp, mg_per_dl, serial_number, trend_arrow) VALUES (?, ?, ?, ?, ?)'
          const params = [
            patientId,
            timestamp,
            mgPerDl,
            serialNumber,
            trendArrow
          ]

          client.execute(query, params, { prepare: true }, (err) => {
            if (err) {
              console.log(err)
            }

            return callback(null)
          })
        })
      }
    ],
    (err) => {
      if (err) {
        console.log(err)
      }

      return setImmediate(() => setTimeout(run, 65000))
    }
  )
}

run()
// setInterval(run, 65000)