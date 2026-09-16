function iso(value) {
  if(value instanceof Date)return value.toISOString();
  const text=String(value).replace(' ','T');
  return new Date(/[Zz]|[+-]\d\d:?\d\d$/.test(text)?text:text+'Z').toISOString();
}
const signature=row=>JSON.stringify([row.field,row.fromValue??null,row.toValue??null,iso(row.createdAt),row.actor??null]);

/** Keep only source identities in observed display order, never response values. */
export function captureOrdering(cases, activities) {
  const links=Object.fromEntries(cases.filter(c=>c.argv[0]==='links'&&c.exitCode===0).map(c=>[c.argv[1],c.payload.map(row=>row.id)]));
  const history={};
  for(const c of cases.filter(c=>c.argv[0]==='history'&&c.argv.length===2&&c.exitCode===0)) {
    const remaining=activities.filter(row=>row.key===c.argv[1]);
    history[c.argv[1]]=c.payload.map(event=>{
      const at=remaining.findIndex(row=>signature(row)===signature(event));
      if(at<0)throw new Error(`Captured history does not match snapshot rows for ${c.argv[1]}`);
      return remaining.splice(at,1)[0].id;
    });
  }
  return {links,history};
}
