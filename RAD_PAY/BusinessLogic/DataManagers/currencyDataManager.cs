using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class currencyDataManager
    {
        //currencyViewModel
        //public long       id          { get; set; }
        //public double?    usd_uzs     { get; set; }
        //public double?    usd_rub     { get; set; }
        //public double?    usd_eur     { get; set; }
        //public DateTime?  upd_ts      { get; set; }
        //public DateTime?  expiry_ts   { get; set; }
        //public int        type        { get; set; }

        public static void Add(currencyViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new currency
            {
                id         = model.id           ,
                usd_uzs    = model.usd_uzs      ,
                usd_rub    = model.usd_rub      ,
                usd_eur    = model.usd_eur      ,
                upd_ts     = model.upd_ts       ,
                expiry_ts  = model.expiry_ts    ,
                type       = model.type         ,
            };

            db.currencies.Add(dbmodel);
        }

        public static void Modify(currencyViewModel model, RAD_PAYEntities db)
        {
            var result = db.currencies.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ;
                    dbmodel.usd_uzs = model.usd_uzs         ;
                    dbmodel.usd_rub = model.usd_rub         ;
                    dbmodel.usd_eur = model.usd_eur         ;
                    dbmodel.upd_ts = model.upd_ts           ;
                    dbmodel.expiry_ts = model.expiry_ts     ;
                    dbmodel.type = model.type;
                }
            }
        }

        public static void Delete(currencyViewModel model, RAD_PAYEntities db)
        {
            var result = db.currencies.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.currencies.Remove(dbmodel);
                }
            }
        }

        public static List<currencyViewModel> Get(currencyViewModel model, RAD_PAYEntities db)
        {
            List<currencyViewModel> list = null;

            var query = from resmodel in db.currencies
                        select new currencyViewModel
                        {
                            id = resmodel.id,
                            usd_uzs = resmodel.usd_uzs,
                            usd_rub = resmodel.usd_rub,
                            usd_eur = resmodel.usd_eur,
                            upd_ts = resmodel.upd_ts,
                            expiry_ts = resmodel.expiry_ts,
                            type = resmodel.type,
                        };

            list = query.ToList();

            return list;
        }
    }
}