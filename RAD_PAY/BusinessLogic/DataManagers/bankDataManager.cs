using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class bankDataManager
    {
        //bankViewModel
        //public long       id          { get; set; }
        //public long?      min_limit   { get; set; }
        //public long?      max_limit   { get; set; }
        //public DateTime?  add_ts      { get; set; }
        //public string     name        { get; set; }
        //public int?       rate        { get; set; }
        //public long?      merchant_id { get; set; }
        //public long?      terminal_id { get; set; }
        //public int?       port        { get; set; }
        //public long?      month_limit { get; set; }
        //public string     offer_link  { get; set; }
        //public int        status      { get; set; }
        //public string     bin_code    { get; set; }
        //public long?      icon_id     { get; set; }

        public static void Add(bankViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new bank
            {
                id          = model.id         ,
                min_limit   = model.min_limit  ,
                max_limit   = model.max_limit  ,
                add_ts      = model.add_ts     ,
                name        = model.name       ,
                rate        = model.rate       ,
                merchant_id = model.merchant_id,
                terminal_id = model.terminal_id,
                port        = model.port       ,
                month_limit = model.month_limit,
                offer_link  = model.offer_link ,
                status      = model.status     ,
                bin_code    = model.bin_code   ,
                icon_id     = model.icon_id    ,
            };

            db.banks.Add(dbmodel);
        }

        public static void Modify(bankViewModel model, RAD_PAYEntities db)
        {
            var result = db.banks.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                       ;
                    dbmodel.min_limit = model.min_limit         ;
                    dbmodel.max_limit = model.max_limit         ;
                    dbmodel.add_ts = model.add_ts               ;
                    dbmodel.name = model.name                   ;
                    dbmodel.rate = model.rate                   ;
                    dbmodel.merchant_id = model.merchant_id     ;
                    dbmodel.terminal_id = model.terminal_id     ;
                    dbmodel.port = model.port                   ;
                    dbmodel.month_limit = model.month_limit     ;
                    dbmodel.offer_link = model.offer_link       ;
                    dbmodel.status = model.status               ;
                    dbmodel.bin_code = model.bin_code           ;
                    dbmodel.icon_id = model.icon_id             ;
                }
            }
        }

        public static void Delete(bankViewModel model, RAD_PAYEntities db)
        {
            var result = db.banks.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.banks.Remove(dbmodel);
                }
            }
        }

        public static List<bankViewModel> Get(bankViewModel model, RAD_PAYEntities db)
        {
            List<bankViewModel> list = null;

            var query = from resmodel in db.banks
                        select new bankViewModel
                        {
                            id = resmodel.id,
                            min_limit = resmodel.min_limit,
                            max_limit = resmodel.max_limit,
                            add_ts = resmodel.add_ts,
                            name = resmodel.name,
                            rate = resmodel.rate,
                            merchant_id = resmodel.merchant_id,
                            terminal_id = resmodel.terminal_id,
                            port = resmodel.port,
                            month_limit = resmodel.month_limit,
                            offer_link = resmodel.offer_link,
                            status = resmodel.status,
                            bin_code = resmodel.bin_code,
                            icon_id = resmodel.icon_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}