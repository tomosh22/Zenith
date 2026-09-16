import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

export function validateInventory(columns, inventory) {
  const entities=new Map(inventory.entities.map(e=>[e.id,e]));
  const errors=[];
  for(const column of columns) {
    const name=`${column.table_schema}.${column.table_name}`;
    const entity=entities.get(name);
    const mapping=entity?.fields.find(f=>f.sql===column.column_name);
    if(!entity || !mapping) errors.push(`Unmapped column: ${name}.${column.column_name}`);
    else if(!mapping.vault && !mapping.reason) errors.push(`Missing disposition: ${name}.${column.column_name}`);
  }
  return errors;
}
if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const [source, mapping]=process.argv.slice(2);
  const data=JSON.parse(readFileSync(source,'utf8'));
  const errors=validateInventory(data.columns??data,JSON.parse(readFileSync(mapping,'utf8')));
  console.log(JSON.stringify({columns:(data.columns??data).length,errors},null,2));
  if(errors.length) process.exitCode=1;
}
