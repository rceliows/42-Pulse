import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer } from 'recharts'

export default function Histogram({ data }) {
  return (
    <ResponsiveContainer width="100%" height={200}>
      <BarChart data={data}>
        <XAxis dataKey="hour_utc" tickFormatter={v => v.slice(11, 16)} />
        <YAxis />
        <Tooltip />
        <Bar dataKey="pulses" fill="#2d9cdb" radius={[4, 4, 0, 0]} />
      </BarChart>
    </ResponsiveContainer>
  )
}